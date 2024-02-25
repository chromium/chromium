// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::config;
use crate::crates::Epoch;
use crate::inherit::{
    find_inherited_privilege_group, find_inherited_security_critical_flag,
    find_inherited_shipped_flag,
};
use crate::metadata_util::{metadata_nodes, metadata_packages};
use crate::paths;
use crate::readme;
use crate::util::{
    create_dirs_if_needed, init_handlebars, remove_checksums_from_lock, run_cargo_metadata,
    run_command, without_cargo_config_toml,
};
use crate::vet::create_vet_config;
use crate::VendorCommandArgs;
use anyhow::{format_err, Context, Result};
use std::collections::{HashMap, HashSet};
use std::path::PathBuf;

pub fn vendor(args: VendorCommandArgs, paths: &paths::ChromiumPaths) -> Result<()> {
    // Vendoring needs to work with real crates.io, not with our locally vendored
    // crates.
    without_cargo_config_toml(paths, || vendor_impl(args, paths))?;
    println!("Vendor successful: run gnrt gen to generate GN rules.");
    Ok(())
}

fn vendor_impl(args: VendorCommandArgs, paths: &paths::ChromiumPaths) -> Result<()> {
    let config_file_path = paths.third_party_config_file;
    let config_file_contents = std::fs::read_to_string(config_file_path).unwrap();
    let config: config::BuildConfig = toml::de::from_str(&config_file_contents).unwrap();

    let readme_template_path = paths
        .third_party_config_file
        .parent()
        .unwrap()
        .join(&config.gn_config.readme_file_template);
    let readme_handlebars = init_handlebars(&readme_template_path).context("init_handlebars")?;

    let vet_template_path = paths //
        .third_party_config_file
        .parent()
        .unwrap()
        .join(&config.vet_config.config_template);
    let vet_handlebars = init_handlebars(&vet_template_path).context("init_handlebars")?;

    println!("Vendoring crates from {}", paths.third_party_cargo_root.display());

    let metadata =
        run_cargo_metadata(paths.third_party_cargo_root.into(), Vec::new(), HashMap::new())
            .context("run_cargo_metadata")?;
    let packages: HashMap<_, _> = metadata_packages(&metadata)?;
    let nodes: HashMap<_, _> = metadata_nodes(&metadata);
    let root = metadata.resolve.as_ref().unwrap().root.as_ref().unwrap();

    // Running cargo commands against actual crates.io will put checksum into
    // the Cargo.lock file, but we don't generate checksums when we download
    // the crates. This mismatch causes everything else to fail when cargo is
    // using our vendor/ directory. So we remove all the checksums from the
    // lock file.
    remove_checksums_from_lock(paths.third_party_cargo_root)?;

    {
        let package_names = packages.values().map(|p| &p.name).collect::<HashSet<_>>();
        for name in config.per_crate_config.keys() {
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
    for p in packages.values() {
        let crate_dir = format!("{}-{}", p.name, p.version);
        if config.resolve.remove_crates.contains(&p.name) {
            println!("Generating placeholder for removed crate {}-{}", p.name, p.version);
            placeholder_crate(p, &nodes, paths, &config)?;
        } else if !dirs.contains(&crate_dir) {
            println!("Downloading {}-{}", p.name, p.version);
            download_crate(&p.name, &p.version, paths)?;
            let skip_patches = match &args.no_patches {
                Some(v) => v.is_empty() || v.contains(&p.name),
                None => false,
            };
            if skip_patches {
                log::warn!("Skipped applying patches for {}-{}", p.name, p.version);
            } else {
                apply_patches(&p.name, &p.version, paths)?
            }
        }
        dirs.remove(&format!("{}-{}", p.name, p.version));
    }
    for d in &dirs {
        println!("Deleting {}", d);
        std::fs::remove_dir_all(paths.third_party_cargo_root.join("vendor").join(d))
            .with_context(|| format!("removing {}", d))?
    }

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

    // Find any epoch dirs which don't correspond to vendored sources anymore,
    // i.e. that are not present in `all_readme_files`.
    for crate_dir in std::fs::read_dir(paths.third_party)? {
        let crate_dir = crate_dir.context("crate_dir")?;
        if !crate_dir.metadata().context("crate_dir metadata")?.is_dir() {
            continue;
        }

        for epoch_dir in std::fs::read_dir(crate_dir.path()).context("read_dir")? {
            let epoch_dir = epoch_dir.context("epoch_dir")?;

            // There are vendored sources for the epoch dir, go to the next.
            if all_readme_files.contains_key(&epoch_dir.path()) {
                continue;
            }

            let is_epoch_name = |n: &str| <Epoch as std::str::FromStr>::from_str(n).is_ok();

            let metadata = epoch_dir.metadata()?;
            if metadata.is_dir() && is_epoch_name(&epoch_dir.file_name().to_string_lossy()) {
                log::warn!(
                    "No vendored sources for '{}', it should be removed.",
                    epoch_dir.path().display()
                );
            }
        }
    }

    let vet_config_toml =
        create_vet_config(packages.values().copied(), &config, find_group, find_shipped)?;

    for dir in all_readme_files.keys() {
        create_dirs_if_needed(dir).context(format!("dir: {}", dir.display()))?;
    }

    if args.dump_template_input {
        serde_json::to_writer_pretty(
            std::fs::File::create(
                paths.vet_config_file.parent().unwrap().join("vet-template-input.json"),
            )
            .context("opening dump file")?,
            &vet_config_toml,
        )
        .context("dumping vet config information")?;

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

    {
        let config_str = vet_handlebars.render("template", &vet_config_toml)?;
        let file_path = paths.vet_config_file;
        let file = std::fs::File::create(paths.vet_config_file).with_context(|| {
            format!("Could not create README.chromium output file {}", file_path.to_string_lossy())
        })?;
        use std::io::Write;
        write!(std::io::BufWriter::new(file), "{}", config_str)
            .with_context(|| format!("{}", file_path.to_string_lossy()))?;
    }

    for (dir, readme_file) in &all_readme_files {
        let readme_str = readme_handlebars.render("template", &readme_file)?;

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
    let crate_dir = vendor_dir.join(format!("{name}-{version}"));

    if let Err(e) = archive.unpack(vendor_dir) {
        std::fs::remove_dir_all(crate_dir)
            .with_context(|| format!("Deleting failed unpack of crate {name}-{version}"))?;
        return Err(e).with_context(|| format!("Failed to unpack crate {name}-{version}"));
    }

    std::fs::write(crate_dir.join(".cargo-checksum.json"), "{\"files\":{}}\n")
        .with_context(|| format!("writing .cargo-checksum.json for crate {}", name))?;

    Ok(())
}

fn apply_patches(
    name: &str,
    version: &semver::Version,
    paths: &paths::ChromiumPaths,
) -> Result<()> {
    let vendor_dir = paths.third_party_cargo_root.join("vendor");
    let crate_dir = vendor_dir.join(format!("{name}-{version}"));

    let mut patches = Vec::new();
    let Ok(patch_dir) = std::fs::read_dir(paths.third_party_cargo_root.join("patches").join(name))
    else {
        // No patches for this crate.
        return Ok(());
    };
    for d in patch_dir {
        patches.push(d?.path());
    }
    patches.sort_unstable();

    let mut patches_contents = Vec::with_capacity(patches.len());
    for path in patches {
        let contents = std::fs::read(&path)?;
        patches_contents.push((path, contents));
    }

    for (path, contents) in patches_contents {
        let mut c = std::process::Command::new("git");
        c.arg("apply");

        // We need to rebase from the old versioned directory to the new one.
        c.arg(format!("-p{}", crate_dir.ancestors().count()));
        c.arg(format!("--directory={}", crate_dir.display()));

        println!("Applying patch {}", path.to_string_lossy());
        if let Err(e) = run_command(c, "patch", Some(&contents)) {
            log::error!("Applying patches failed! Removing the {} directory.", crate_dir.display());
            if let Err(rm_err) = std::fs::remove_dir_all(&crate_dir) {
                Err(rm_err).context(e)?
            } else {
                Err(e)?
            }
        }
    }

    Ok(())
}

fn placeholder_crate(
    package: &cargo_metadata::Package,
    nodes: &HashMap<&cargo_metadata::PackageId, &cargo_metadata::Node>,
    paths: &paths::ChromiumPaths,
    config: &config::BuildConfig,
) -> Result<()> {
    let removed_cargo_template_path = paths
        .third_party_config_file
        .parent()
        .unwrap()
        .join(&config.gn_config.removed_cargo_template);
    let removed_cargo_template =
        init_handlebars(&removed_cargo_template_path).context("loading removed_cargo_template")?;
    let removed_librs_template_path = paths
        .third_party_config_file
        .parent()
        .unwrap()
        .join(&config.gn_config.removed_librs_template);
    let removed_librs_template =
        init_handlebars(&removed_librs_template_path).context("loading removed_librs_template")?;

    let vendor_dir = paths.third_party_cargo_root.join("vendor");
    let crate_dir = vendor_dir.join(format!("{}-{}", package.name, package.version));

    create_dirs_if_needed(&crate_dir).context("creating crate dir")?;
    for x in std::fs::read_dir(&crate_dir)? {
        let entry = x?;
        if entry.metadata()?.is_dir() {
            std::fs::remove_dir_all(&entry.path())
                .with_context(|| format!("removing dir {}", entry.path().display()))?;
        } else {
            std::fs::remove_file(&entry.path())
                .with_context(|| format!("removing file {}", entry.path().display()))?;
        }
    }

    #[derive(serde::Serialize)]
    struct PlaceholderCrate<'a> {
        name: &'a str,
        version: &'a semver::Version,
        dependencies: Vec<PlaceholderDependency<'a>>,
        features: Vec<String>,
    }
    #[derive(serde::Serialize)]
    struct PlaceholderDependency<'a> {
        name: &'a str,
        version: &'a str,
        default_features: bool,
        features: Vec<&'a str>,
    }

    let node = nodes[&package.id];

    // We need to collect all dependencies of the crate so they can be
    // reproduced in the placeholder Cargo.toml file. Otherwise the
    // Cargo.lock may be considered out of date and cargo will try
    // to rewrite it to remove the missing dependencies.
    //
    // However we don't just want all dependencies that are listed in
    // the Cargo.toml since they may be optional and not enabled by a
    // feature in our build. In that case, cargo would want to add the
    // new dependencies to the Cargo.lock.
    //
    // So we use the [`cargo_metadata::Node`] to find the resolved set
    // of dependencies that are actually in use (in build or in prod).
    //
    // Since features (at this time) can not be changed per-platform,
    // the resolved [`cargo_metadata::Node`] does not have feature
    // information about dependencies. We grab that verbatim from the
    // Cargo.toml through the [`cargo_metadata::Dependency`] type which
    // we call `feature_dep_info`.
    let dependencies = node
        .deps
        .iter()
        .filter(|d| {
            d.dep_kinds.iter().any(|k| {
                k.kind == cargo_metadata::DependencyKind::Build
                    || k.kind == cargo_metadata::DependencyKind::Normal
            })
        })
        .map(|d| {
            let feature_dep_info = package
                .dependencies
                .iter()
                .find(|f| f.name == d.name)
                .expect("dependency not in list");
            PlaceholderDependency {
                name: &d.name,
                version: "*",
                default_features: feature_dep_info.uses_default_features,
                features: feature_dep_info.features.iter().map(String::as_str).collect(),
            }
        })
        .collect();

    let mut features = package.features.keys().cloned().collect::<Vec<String>>();
    features.sort_unstable();

    let placeholder_crate =
        PlaceholderCrate { name: &package.name, version: &package.version, dependencies, features };

    {
        let rendered = removed_cargo_template.render("template", &placeholder_crate)?;
        let file_path = crate_dir.join("Cargo.toml");
        let file = std::fs::File::create(&file_path).with_context(|| {
            format!("Could not create Cargo.toml output file {}", file_path.to_string_lossy())
        })?;
        use std::io::Write;
        write!(std::io::BufWriter::new(file), "{}", rendered)
            .with_context(|| format!("{}", file_path.to_string_lossy()))?;
    }

    create_dirs_if_needed(&crate_dir.join("src")).context("creating src dir")?;
    {
        let rendered = removed_librs_template.render("template", &placeholder_crate)?;
        let file_path = crate_dir.join("src").join("lib.rs");
        let file = std::fs::File::create(&file_path).with_context(|| {
            format!("Could not create lib.rs output file {}", file_path.to_string_lossy())
        })?;
        use std::io::Write;
        write!(std::io::BufWriter::new(file), "{}", rendered)
            .with_context(|| format!("{}", file_path.to_string_lossy()))?;
    }

    std::fs::write(crate_dir.join(".cargo-checksum.json"), "{\"files\":{}}\n")
        .with_context(|| format!("writing .cargo-checksum.json for crate {}", package.name))?;

    Ok(())
}
