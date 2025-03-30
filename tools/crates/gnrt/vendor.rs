// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::config;
use crate::crates::Epoch;
use crate::deps;
use crate::inherit::{
    find_inherited_privilege_group, find_inherited_security_critical_flag,
    find_inherited_shipped_flag,
};
use crate::metadata_util::{metadata_nodes, metadata_packages};
use crate::paths::{self, get_vendor_dir_for_package};
use crate::readme;
use crate::util::{
    create_dirs_if_needed, get_guppy_package_graph, init_handlebars, remove_checksums_from_lock,
    run_cargo_metadata, run_command, without_cargo_config_toml,
};
use crate::vet::create_vet_config;
use crate::VendorCommandArgs;

use anyhow::{format_err, Context, Result};

use std::collections::{HashMap, HashSet};
use std::path::{Path, PathBuf};

pub fn vendor(args: VendorCommandArgs, paths: &paths::ChromiumPaths) -> Result<()> {
    // Vendoring needs to work with real crates.io, not with our locally vendored
    // crates.
    without_cargo_config_toml(paths, || vendor_impl(args, paths))?;
    println!("Vendor successful: run gnrt gen to generate GN rules.");
    Ok(())
}

fn vendor_impl(args: VendorCommandArgs, paths: &paths::ChromiumPaths) -> Result<()> {
    let config_file_path = paths.third_party_config_file;
    let config = config::BuildConfig::from_path(config_file_path)?;

    // `unwrap` ok, because `BuildConfig::from_path` would have failed if there is
    // no parent.
    let third_party_dir = paths.third_party_config_file.parent().unwrap();
    let readme_template_path = third_party_dir.join(&config.gn_config.readme_file_template);
    let readme_handlebars = init_handlebars(&readme_template_path).context("init_handlebars")?;
    let vet_template_path = third_party_dir.join(&config.vet_config.config_template);
    let vet_handlebars = init_handlebars(&vet_template_path).context("init_handlebars")?;

    println!("Vendoring crates from {}", paths.third_party_cargo_root.display());

    let metadata =
        run_cargo_metadata(paths.third_party_cargo_root.into(), Vec::new(), HashMap::new())
            .context("run_cargo_metadata")?;
    let packages: HashMap<_, _> = metadata_packages(&metadata)?;
    let nodes: HashMap<_, _> = metadata_nodes(&metadata);
    let root = metadata.resolve.as_ref().unwrap().root.as_ref().unwrap();

    let guppy_resolved_package_ids = get_guppy_resolved_package_ids(&config, paths)?;
    let is_removed = |cargo_package_id: &cargo_metadata::PackageId| -> bool {
        let p = packages[cargo_package_id];
        config.resolve.remove_crates.contains(&p.name)
            || !guppy_resolved_package_ids.contains(&p.into())
    };

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
    let mut dirs_to_remove: HashSet<String> = std::fs::read_dir(&vendor_dir)
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
        let crate_dir = get_vendor_dir_for_package(&p.name, &p.version);

        // Keep directories corresponding to packages from the dependency tree.
        dirs_to_remove.remove(&crate_dir);

        let is_already_vendored = get_package_id_from_vendored_dir(&vendor_dir.join(&crate_dir))
            .filter(|vendored| vendored.name() == p.name && *vendored.version() == p.version)
            .is_some();
        if is_already_vendored {
            continue;
        }

        if is_removed(&p.id) {
            println!("Generating placeholder for removed crate {}", &crate_dir);
            generate_placeholder_crate(p, &packages, &nodes, paths, &config)?;
        } else {
            println!("Downloading {}", &crate_dir);
            download_crate(&p.name, &p.version, paths)?;
            let skip_patches = match &args.no_patches {
                Some(v) => v.is_empty() || v.contains(&p.name),
                None => false,
            };
            if skip_patches {
                log::warn!("Skipped applying patches for {}", &crate_dir);
            } else {
                apply_patches(&p.name, &p.version, paths)?
            }
        }
    }
    for d in &dirs_to_remove {
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
                // Don't generate README files for removed packages.
                .filter(|p| !is_removed(&p.id))
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
                let path = epoch_dir.path();
                println!("Deleting {}", path.display());
                std::fs::remove_dir_all(&path)
                    .with_context(|| format!("removing {}", path.display()))?
            }
        }
    }

    let vet_config_toml =
        create_vet_config(packages.values().copied(), is_removed, find_group, find_shipped)?;

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
            format!("Could not create vet config output file {}", file_path.to_string_lossy())
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

fn get_guppy_resolved_package_ids(
    config: &config::BuildConfig,
    paths: &paths::ChromiumPaths,
) -> Result<HashSet<deps::PackageId>> {
    // `gnrt vendor` (unlike `gnrt gen`) doesn't need to pass `--offline` nor
    // `--locked` to `cargo`.
    let cargo_extra_options = vec![];
    let dependencies = deps::collect_dependencies(
        &get_guppy_package_graph(
            paths.third_party_cargo_root.into(),
            cargo_extra_options,
            HashMap::new(),
        )?,
        &config.resolve.root,
        config,
    )?;
    Ok(dependencies.iter().map(|p| p.into()).collect())
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
        if let Some(value) = header {
            value.to_str()?.parse::<usize>()?
        } else {
            0
        }
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

    // Using `TempDir::with_prefix_in` to ensure that `std::fs::rename` below
    // doesn't need to work across mount points / across filesystems.
    let vendor_dir = paths.third_party_cargo_root.join("vendor");
    let tempdir = tempfile::TempDir::with_prefix_in("tmp-gnrt-vendor", &vendor_dir)?;
    archive.unpack(tempdir.path())?;

    // Remove old vendored dir (most likely an old version of the crate).
    let crate_dir = vendor_dir.join(get_vendor_dir_for_package(name, version));
    std::fs::remove_dir_all(&crate_dir).or_else(|err| {
        if err.kind() == std::io::ErrorKind::NotFound {
            // Ignore errors if the directory already didn't exist.
            Ok(())
        } else {
            Err(err)
        }
    })?;

    // Expecting that the archive will contain a directory with a predictable
    // path based on crate name and version.  Move this directory to the final
    // destination (to `crate_dir`).
    let archived_dir = tempdir.path().join(format!("{}-{}", name, version));
    std::fs::rename(archived_dir, &crate_dir)?;

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
    let crate_dir = vendor_dir.join(get_vendor_dir_for_package(name, version));

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
        let args = vec![
            "apply".to_string(),
            // We need to rebase from the old versioned directory to the new one.
            format!("-p{}", crate_dir.ancestors().count()),
            format!("--directory={}", crate_dir.display()),
        ];
        let mut c = std::process::Command::new("git");
        c.args(args.clone());

        println!("Applying patch {}", path.to_string_lossy());
        if let Err(e) = run_command(c, "patch", Some(&contents)) {
            log::error!(
                "Applying patches failed - retrying with verbose output to help diagnose..."
            );
            let mut c = std::process::Command::new("git");
            c.args(args);
            c.arg("-v");
            let _ignoring_error = run_command(c, "patch", Some(&contents));

            log::error!(
                "Applying patches failed - cleaning up: Removing the {} directory.",
                crate_dir.display(),
            );
            if let Err(rm_err) = std::fs::remove_dir_all(&crate_dir) {
                Err(rm_err).context(e)?
            } else {
                Err(e)?
            }
        }
    }

    Ok(())
}

#[derive(serde::Serialize)]
struct PlaceholderCrate<'a> {
    name: &'a str,
    version: &'a semver::Version,
    dependencies: Vec<PlaceholderDependency<'a>>,
    features: Vec<String>,
}
#[derive(Debug, serde::Serialize)]
struct PlaceholderDependency<'a> {
    name: &'a str,
    version: String,
    default_features: bool,
    features: Vec<&'a str>,
}

fn get_placeholder_crate_metadata<'a>(
    package_id: &'a cargo_metadata::PackageId,
    packages: &'a HashMap<&cargo_metadata::PackageId, &cargo_metadata::Package>,
    nodes: &HashMap<&'a cargo_metadata::PackageId, &'a cargo_metadata::Node>,
) -> PlaceholderCrate<'a> {
    let node = nodes[package_id];
    let package = packages[package_id];

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
            // Translate `proc_macro2` (dependency name) into `proc-macro2` (package name).
            let dep_name = &packages[&d.pkg].name;
            let feature_dep_info =
                package.dependencies.iter().find(|f| f.name == *dep_name).unwrap_or_else(|| {
                    panic!(
                        "`{}` (`{}`) is not listed as a dependency of `{}-{}`",
                        dep_name, d.pkg, package.name, package.version,
                    )
                });
            PlaceholderDependency {
                name: dep_name,
                version: feature_dep_info.req.to_string(),
                default_features: feature_dep_info.uses_default_features,
                features: feature_dep_info.features.iter().map(String::as_str).collect(),
            }
        })
        .collect();

    let mut features = package.features.keys().cloned().collect::<Vec<String>>();
    features.sort_unstable();

    PlaceholderCrate { name: &package.name, version: &package.version, dependencies, features }
}

fn generate_placeholder_crate(
    package: &cargo_metadata::Package,
    packages: &HashMap<&cargo_metadata::PackageId, &cargo_metadata::Package>,
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
    let crate_dir = vendor_dir.join(get_vendor_dir_for_package(&package.name, &package.version));
    create_dirs_if_needed(&crate_dir).context("creating crate dir")?;
    for x in std::fs::read_dir(&crate_dir)? {
        let entry = x?;
        if entry.metadata()?.is_dir() {
            std::fs::remove_dir_all(entry.path())
                .with_context(|| format!("removing dir {}", entry.path().display()))?;
        } else {
            std::fs::remove_file(entry.path())
                .with_context(|| format!("removing file {}", entry.path().display()))?;
        }
    }

    let placeholder_crate = get_placeholder_crate_metadata(&package.id, packages, nodes);

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

/// Parses `dir/Cargo.toml` to extract package name and version.
///
/// This is intended to be used during the vendoring process, to determine
/// if an existing `third_party/rust/chromium_crates_io/vendor/foo` directory
/// contains an up-to-date version of a crate.
fn get_package_id_from_vendored_dir(dir: &Path) -> Option<deps::PackageId> {
    // Using manual, non-strongly-typed TOML parsing (instead of going through
    // `cargo metadata` or `guppy`) to work even if `Cargo.lock` is absent
    // (in this situation `cargo --locked --offline` would complain).
    let file_contents = std::fs::read_to_string(dir.join("Cargo.toml")).ok()?;
    let toml = file_contents.parse::<toml::value::Table>().ok()?;
    let package = toml.get("package")?.as_table()?;
    let name = package.get("name")?.as_str()?;
    let version = package.get("version")?.as_str()?.parse().ok()?;
    Some(deps::PackageId::new(name.to_string(), version))
}

#[cfg(test)]
mod test {
    use super::*;

    use semver::Version;
    use tempfile::TempDir;

    #[test]
    fn test_get_package_id_from_vendored_dir_for_happy_case() {
        // Create a dir with only `Cargo.toml` and `src/lib.rs`, because these are the
        // only files that exist in "placeholder" crates (ones that `gnrt`
        // conjures to replace crates trimmed from the dependency tree).
        let crate_dir = TempDir::with_prefix("gnrt_unittests").unwrap();
        std::fs::write(
            crate_dir.path().join("Cargo.toml"),
            r#"
            [package]
            name = "some_package"
            version = "1.2.3"
        "#,
        )
        .unwrap();
        let src_dir = crate_dir.path().join("src");
        std::fs::create_dir(&src_dir).unwrap();
        std::fs::write(src_dir.join("lib.rs"), "").unwrap();

        // Check that `get_package_id_from_vendored_dir` can detect the crate name and
        // version.
        let Some(package_id) = get_package_id_from_vendored_dir(crate_dir.path()) else {
            panic!("`None` returned from get_package_id_from_vendored_dir");
        };
        assert_eq!(package_id.name(), "some_package");
        assert_eq!(*package_id.version(), Version::new(1, 2, 3));
    }

    #[test]
    fn test_get_placeholder_crate_metadata_with_proc_macro2_dependency() {
        let metadata: cargo_metadata::Metadata =
            serde_json::from_str(SAMPLE_CARGO_METADATA2).unwrap();
        let packages: HashMap<_, _> = metadata_packages(&metadata).unwrap();
        let nodes: HashMap<_, _> = metadata_nodes(&metadata);
        let zerocopy_derive = packages.values().find(|p| p.name == "yoke-derive").unwrap();
        let placeholder = get_placeholder_crate_metadata(&zerocopy_derive.id, &packages, &nodes);
        assert_eq!(placeholder.name, "yoke-derive");
        assert_eq!(placeholder.version.to_string(), "0.8.0");
        assert!(placeholder.features.is_empty());

        let mut i = 0;
        assert_eq!(placeholder.dependencies[i].name, "proc-macro2");
        assert_eq!(placeholder.dependencies[i].version, "^1.0.61");
        assert!(placeholder.dependencies[i].default_features);
        assert!(placeholder.dependencies[i].features.is_empty());

        i += 1;
        assert_eq!(placeholder.dependencies[i].name, "quote");
        assert_eq!(placeholder.dependencies[i].version, "^1.0.28");
        assert!(placeholder.dependencies[i].default_features);
        assert!(placeholder.dependencies[i].features.is_empty());

        i += 1;
        assert_eq!(placeholder.dependencies[i].name, "syn");
        assert_eq!(placeholder.dependencies[i].version, "^2.0.21");
        assert!(placeholder.dependencies[i].default_features);
        assert_eq!(placeholder.dependencies[i].features, &["fold"]);

        i += 1;
        assert_eq!(placeholder.dependencies[i].name, "synstructure");
        assert_eq!(placeholder.dependencies[i].version, "^0.13.0");
        assert!(placeholder.dependencies[i].default_features);
        assert!(placeholder.dependencies[i].features.is_empty());

        i += 1;
        assert_eq!(placeholder.dependencies.len(), i);
    }

    // `test_metadata2.json` contains the output of `cargo metadata` run in
    // `gnrt/sample_package2` directory.  See the `Cargo.toml` for more
    // information.
    static SAMPLE_CARGO_METADATA2: &str = include_str!("lib/test_metadata2.json");
}
