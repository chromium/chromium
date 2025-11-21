// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::config;
use crate::crates::{self, Epoch};
use crate::deps;
use crate::inherit::{
    find_inherited_privilege_group, find_inherited_security_critical_flag,
    find_inherited_shipped_flag,
};
use crate::paths::{self, get_build_dir_for_package, get_vendor_dir_for_package};
use crate::readme::{self, ReadmeFile};
use crate::toml_edit_utils;
use crate::unsafe_code_detector;
use crate::util::{
    create_dirs_if_needed, get_guppy_package_graph, init_handlebars,
    init_handlebars_with_template_paths, remove_checksums_from_lock, render_handlebars,
    render_handlebars_named_template, run_command, without_cargo_config_toml,
};
use crate::VendorCommandArgs;

use anyhow::{bail, format_err, Context, Result};
use guppy::graph::PackageMetadata;
use itertools::Itertools;

use std::collections::{HashMap, HashSet};
use std::ffi::OsString;
use std::fs::File;
use std::io::Write;
use std::path::{Path, PathBuf};

/// `fn vendor` implements handling of the `gnrt vendor` CLI command - this
/// covers the following steps:
///
/// 1. Downloading missing dependencies from https://crates.io into
///    `third_party/rust/chromium_crates_io/vendor/`
///     - Using `cargo` / `guppy` (in online mode) to resolve transitive
///       dependencies of `third_party/rust/chromium_crates_io/Cargo.toml`
///     - Downloading and extracting crate sources
///     - Applying patches from `third_party/rust/chromium_crates_io/patches/`
/// 2. Generating additional Chromium metadata for all dependencies:
///     - Using `cargo` / `guppy` (in offline mode) to resolve transitive
///       dependencies of `third_party/rust/chromium_crates_io/Cargo.toml`
///     - Generating `README.chromium` files
///     - Editing `third_party/rust/chromium_crates_io/gnrt_config.toml` to
///       ensure that all dependencies specify `allow_unsafe = ...` (this step
///       is meant to streamline the import experience and unblock building
///       unsafe code, but it is based on heuristics and may sometimes fail to
///       determine correct settings)."#)]
pub fn vendor(args: VendorCommandArgs, paths: &paths::ChromiumPaths) -> Result<()> {
    // Vendoring needs to work with real crates.io, not with our locally vendored
    // crates.
    without_cargo_config_toml(paths, || download_crates(&args, paths))?;

    // Updating metadata should be performed on the locally vendored crates
    update_vendored_metadata(&args, paths)?;

    println!("Vendor successful: run gnrt gen to generate GN rules.");
    Ok(())
}

fn download_crates(args: &VendorCommandArgs, paths: &paths::ChromiumPaths) -> Result<()> {
    let config_file_path = paths.third_party_config_file;
    let config = config::BuildConfig::from_path(config_file_path)?;

    println!("Vendoring crates from {}", paths.third_party_cargo_root.display());

    let graph =
        get_guppy_package_graph(paths.third_party_cargo_root.into(), vec![], HashMap::new())?;

    let guppy_resolved_package_ids: HashSet<deps::PackageId> =
        deps::collect_dependencies(&graph, &config.resolve.root, &config)?
            .iter()
            .map(|p| p.into())
            .collect();
    let is_removed = |guppy_package_id: &guppy::PackageId| -> bool {
        let p = graph.metadata(guppy_package_id).unwrap();
        config.resolve.remove_crates.contains(p.name())
            || !guppy_resolved_package_ids.contains(&(&p).into())
    };

    // Running cargo commands against actual crates.io will put checksum into
    // the Cargo.lock file, but we don't generate checksums when we download
    // the crates. This mismatch causes everything else to fail when cargo is
    // using our vendor/ directory. So we remove all the checksums from the
    // lock file.
    remove_checksums_from_lock(paths.third_party_cargo_root)?;

    // Download missing dirs, remove the rest.
    let vendor_dir = paths.third_party_cargo_root.join("vendor");
    create_dirs_if_needed(&vendor_dir).context("creating vendor dir")?;
    let mut dirs_to_remove: HashSet<OsString> = std::fs::read_dir(&vendor_dir)
        .context("reading vendor dir")?
        .filter_map(|entry| {
            entry
                .ok()
                .filter(|entry| entry.metadata().map(|m| m.is_dir()).unwrap_or(false))
                .map(|entry| vendor_dir.join(entry.file_name()).as_os_str().to_os_string())
        })
        .collect();
    for p in graph.packages() {
        // Skip if it's the workspace package, since this only exists to have a
        // cargo context.
        if p.in_workspace() {
            continue;
        }

        // In theory we could use a different `crate_dir` for placeholders (e.g.
        // `some-crate-v1-placeholder` rather than `some-crate-v1`), but always using
        // the same name simplifies other tooling (e.g. how
        // `create_update_cl.py` calculates the vendored directory).
        let crate_path = get_vendor_dir_for_package(paths, p.name(), p.version());
        let crate_dirname = crate_path.file_name().unwrap();

        // Keep directories corresponding to packages from the dependency tree.
        dirs_to_remove.remove(crate_path.as_os_str());

        let is_already_right_version =
            get_package_id_from_vendored_dir(&crate_path).is_some_and(|vendored| {
                let expected_name = p.name();
                let expected_version = p.version();
                vendored.name() == expected_name && vendored.version() == expected_version
            });
        let is_already_right_placeholder_status = {
            let expecting_placeholder = is_removed(p.id());
            let vendored_is_placeholder = is_placeholder_crate(&crate_path);
            expecting_placeholder == vendored_is_placeholder
        };
        if is_already_right_version && is_already_right_placeholder_status {
            continue;
        }

        if is_removed(p.id()) {
            let msg =
                format!("Generating placeholder for removed crate {}", crate_dirname.display());
            println!("{msg}");
            generate_placeholder_crate(p, &crate_path).context(msg)?;
        } else {
            let msg = format!("Downloading {}", crate_dirname.display());
            println!("{msg}");
            download_crate(p.name(), p.version(), paths).context(msg)?;
            let skip_patches = match &args.no_patches {
                Some(v) => v.is_empty() || v.iter().any(|x| *x == p.name()),
                None => false,
            };
            if skip_patches {
                log::warn!("Skipped applying patches for {}", crate_dirname.display());
            } else {
                apply_patches(p.name(), p.version(), paths).context(
                    "Applying patches failed - hopefully \
                     `third_party/rust/chromium_crates_io/patches/README.md` \
                     provides some useful guidance for the next steps...",
                )?;
            }
            forward_to_owners_file_in_build_dir(paths, p)?;
        }
    }
    for d in &dirs_to_remove {
        let msg = format!("Deleting {}", d.display());
        println!("{msg}");
        std::fs::remove_dir_all(d).context(msg)?;
    }
    Ok(())
}

fn update_vendored_metadata(args: &VendorCommandArgs, paths: &paths::ChromiumPaths) -> Result<()> {
    let config_file_path = paths.third_party_config_file;
    let config = config::BuildConfig::from_path(config_file_path)?;

    // Fetch the package graph again based on the locally vendored crates, to ensure
    // that locally applied patches which impact the package graph are considered.
    // Although --offline is passed, this function also expects to be executed
    // with a cargo config.toml that uses the locally vendored crates.
    let graph = get_guppy_package_graph(
        paths.third_party_cargo_root.into(),
        vec!["--offline".to_string()],
        HashMap::new(),
    )?;
    let root = match graph.query_workspace().initials().exactly_one() {
        Ok(root) => root,
        Err(_) => anyhow::bail!("cargo workspace must contain exactly one package"),
    }
    .id();
    let used_packages = {
        let guppy_resolved_package_ids: HashSet<deps::PackageId> =
            deps::collect_dependencies(&graph, &config.resolve.root, &config)?
                .iter()
                .map(|p| p.into())
                .collect();
        graph
            .packages()
            .filter(|meta: &PackageMetadata| {
                !config.resolve.remove_crates.contains(meta.name())
                    && guppy_resolved_package_ids.contains(&meta.into())
            })
            .collect_vec()
    };

    let find_group = |id| find_inherited_privilege_group(id, root, &graph, &config);
    let find_security_critical =
        |id| find_inherited_security_critical_flag(id, root, &graph, &config);
    let find_shipped = |id| find_inherited_shipped_flag(id, root, &graph, &config);
    let all_readme_files: HashMap<PathBuf, readme::ReadmeFile> =
        readme::readme_files_from_packages(
            used_packages.iter(),
            paths,
            &config,
            find_group,
            find_security_critical,
            find_shipped,
        )?;

    remove_stale_build_directories(paths, all_readme_files.keys().cloned().collect())?;
    generate_readme_files(&config, paths, &all_readme_files, args.dump_template_input)?;

    let dependencies = deps::collect_dependencies(&graph, &config.resolve.root, &config)?;
    fill_allow_unsafe_settings(&config, paths, dependencies)?;

    Ok(())
}

/// Removes stale `//third_party/rust/<no_longer_needed_crate_dir>` directories.
fn remove_stale_build_directories(
    paths: &paths::ChromiumPaths,
    used_dirs: HashSet<PathBuf>,
) -> Result<()> {
    for crate_dir in std::fs::read_dir(paths.third_party)? {
        let crate_dir = crate_dir.context("crate_dir")?;
        if !crate_dir.metadata().context("crate_dir metadata")?.is_dir() {
            continue;
        }

        for epoch_dir in std::fs::read_dir(crate_dir.path()).context("read_dir")? {
            let epoch_dir = epoch_dir.context("epoch_dir")?;

            // There are vendored sources for the epoch dir, go to the next.
            if used_dirs.contains(&epoch_dir.path()) {
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

    Ok(())
}

/// Generates `//third_party/rust/<crate>/<epoch>/README.chromium` files.
fn generate_readme_files(
    config: &config::BuildConfig,
    paths: &paths::ChromiumPaths,
    all_readme_files: &HashMap<PathBuf, ReadmeFile>,
    dump_template_input: bool,
) -> Result<()> {
    // `unwrap` ok, because `BuildConfig::from_path` would have failed if there is
    // no parent.
    let third_party_dir = paths.third_party_config_file.parent().unwrap();
    let readme_template_path = third_party_dir.join(&config.gn_config.readme_file_template);
    let handlebars = init_handlebars_with_template_paths(&[&readme_template_path])
        .context("init_handlebars for `README.chromium.hbs")?;

    for dir in all_readme_files.keys() {
        create_dirs_if_needed(dir).context(format!("dir: {}", dir.display()))?;
    }

    if dump_template_input {
        for (dir, readme_file) in all_readme_files.iter() {
            serde_json::to_writer_pretty(
                std::fs::File::create(dir.join("gnrt-template-input.json"))
                    .context("opening dump file")?,
                &readme_file,
            )
            .context("dumping gn information")?;
        }
        return Ok(());
    }

    for (dir, readme_file) in all_readme_files {
        render_handlebars(
            &handlebars,
            &readme_template_path,
            &readme_file,
            &dir.join("README.chromium"),
        )?;
    }

    Ok(())
}

/// Edits `//third_party/rust/chromium_crates_io/gnrt_config.toml` to ensure
/// that `allow_unsafe` is provided for all dependencies.  See
/// https://crbug.com/460814809 for the motivation behind this functionality.
fn fill_allow_unsafe_settings(
    config: &config::BuildConfig,
    paths: &paths::ChromiumPaths,
    deps: impl IntoIterator<Item = deps::Package>,
) -> Result<()> {
    use toml_edit::{DocumentMut, InlineTable, Item, Table, TableLike};
    use toml_edit_utils::{format, GNRT_CONFIG_FORMAT_OPTIONS};

    fn get_or_insert_table<'doc>(
        parent: &'doc mut dyn TableLike,
        key: &str,
        new_table_inserter: impl Fn() -> Item,
    ) -> &'doc mut dyn TableLike {
        let item = parent.entry(key).or_insert_with(&new_table_inserter);
        if !item.is_table_like() {
            *item = new_table_inserter();
        }
        item.as_table_like_mut().unwrap()
    }

    let mut did_make_edits = false;
    let mut doc = {
        let input = std::fs::read_to_string(paths.third_party_config_file)?;
        input.parse::<DocumentMut>()?
    };

    let new_table_fn = || Item::from(Table::new());
    let new_inline_table_fn = || Item::from(InlineTable::new());
    let top_table = get_or_insert_table(&mut doc as &mut Table, "crate", new_table_fn);
    for package in deps.into_iter() {
        // TODO(https://crbug.com/419104870): In the future an epoch-based `crate_key`
        // may need to be consulted (in addition-to, or instead-of the `crate_key`
        // below).
        let crate_key = &package.package_name;
        let crate_table = get_or_insert_table(top_table, crate_key, new_table_fn);
        let extra_kv_table = get_or_insert_table(crate_table, "extra_kv", new_inline_table_fn);

        extra_kv_table.entry("allow_unsafe").or_insert_with(|| {
            did_make_edits = true;

            // `unwrap_or` translates errors into `false`, because incorrect
            // `allow_unsafe = false` will be auto-detected via build failures
            // (unlike incorrect `allow_unsafe = true`).
            does_package_contain_unsafe_code(config, &package).unwrap_or(false).into()
        });
    }

    if did_make_edits {
        format(&mut doc, &GNRT_CONFIG_FORMAT_OPTIONS);
        write!(File::create(paths.third_party_config_file)?, "{doc}")?;
    }

    Ok(())
}

fn does_package_contain_unsafe_code(
    config: &config::BuildConfig,
    package: &deps::Package,
) -> Result<bool> {
    let (_, crate_files) =
        crates::collect_crate_files(package, config, crates::IncludeCrateTargets::LibOnly)?;

    crate_files
        .sources
        .iter()
        .chain(crate_files.inputs.iter())
        .chain(crate_files.build_script_sources.iter())
        .chain(crate_files.build_script_inputs.iter())
        .map(|path| does_file_contain_unsafe_code(path))
        .fold_ok(false, |lhs, rhs| lhs || rhs)
}

fn does_file_contain_unsafe_code(path: &Path) -> Result<bool> {
    // `path`-based checks are done first, because they are faster
    // than `file_contents`-based checks.
    let Some(path_as_str) = path.to_str() else { bail!("Non-UTF8 path: {}", path.display()) };
    const FILENAME_SUBSTRINGS_TO_IGNORE: &[&str] = &["bench", "example", "fuzz", "test"];
    if FILENAME_SUBSTRINGS_TO_IGNORE.iter().any(|pattern| path_as_str.contains(pattern)) {
        return Ok(false);
    }

    let file_contents = std::fs::read_to_string(path)?;
    Ok(unsafe_code_detector::contains_unsafe_code(&file_contents))
}

fn download_crate(
    name: &str,
    version: &semver::Version,
    paths: &paths::ChromiumPaths,
) -> Result<()> {
    let mut response = {
        // https://crates.io/data-access#api asks to provide a user agent as a courtesy:
        static USER_AGENT: &str = "gnrt by rust-dev@chromium.org";
        let client = reqwest::blocking::Client::builder().user_agent(USER_AGENT).build()?;

        let download_url = format!("https://crates.io/api/v1/crates/{name}/{version}/download");
        client.get(download_url).send()?
    };
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
            .with_context(|| format!("reading http response for crate {name}"))?;
    }
    let unzipped = flate2::read::GzDecoder::new(bytes.as_slice());
    let mut archive = tar::Archive::new(unzipped);

    // Using `TempDir::with_prefix_in` to ensure that `std::fs::rename` below
    // doesn't need to work across mount points / across filesystems.
    let tempdir =
        tempfile::TempDir::with_prefix_in("tmp-gnrt-vendor", paths.third_party_cargo_root)?;
    archive
        .unpack(tempdir.path())
        .with_context(|| format!("unpacking http response for crate {name}"))?;

    // Remove old vendored dir (most likely an old version of the crate).
    let crate_dir = get_vendor_dir_for_package(paths, name, version);
    std::fs::remove_dir_all(&crate_dir)
        .or_else(|err| {
            if err.kind() == std::io::ErrorKind::NotFound {
                // Ignore errors if the directory already didn't exist.
                Ok(())
            } else {
                Err(err)
            }
        })
        .with_context(|| format!("Removing old vendored sources at {}", crate_dir.display()))?;

    // Expecting that the archive will contain a directory with a predictable
    // path based on crate name and version.  Move this directory to the final
    // destination (to `crate_dir`).
    let archived_dir = tempdir.path().join(format!("{name}-{version}"));
    std::fs::rename(&archived_dir, &crate_dir).with_context(|| {
        format!(
            "Moving unpacked crate contents from {} to {}",
            archived_dir.display(),
            crate_dir.display(),
        )
    })?;

    std::fs::write(crate_dir.join(".cargo-checksum.json"), "{\"files\":{}}\n")
        .with_context(|| format!("writing .cargo-checksum.json for crate {name}"))?;

    Ok(())
}

fn apply_patches(
    name: &str,
    version: &semver::Version,
    paths: &paths::ChromiumPaths,
) -> Result<()> {
    let crate_dir = get_vendor_dir_for_package(paths, name, version);

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

/// Checks if `//third_party/rust/crate_name/OWNERS` exists and if it does,
/// then creates
/// `//third_party/rust/chromium_crates_io/vendor/crate_name-v123/OWNERS`
/// which forward to the former `OWNERS` file.
fn forward_to_owners_file_in_build_dir(
    paths: &paths::ChromiumPaths,
    package: guppy::graph::PackageMetadata,
) -> Result<()> {
    let build_dir = get_build_dir_for_package(paths, package.name(), package.version());

    // We could in theory check first `//t/r/crate_name/v1/OWNERS` (in addition to
    // checking `//t/r/crate_name/OWNERS` as we already do below).  We don't do
    // this because the epoch-specific dirs are auto-deleted by `gnrt` when the
    // epoch goes away.  (i.e. we expect non-generated files to be outside of
    // the epoch-specific dirs).
    let build_dir_owners_file = build_dir.parent().unwrap().join("OWNERS");
    if std::fs::exists(&build_dir_owners_file).unwrap_or(false) {
        use std::io::Write;
        let vendor_dir = get_vendor_dir_for_package(paths, package.name(), package.version());
        let mut f = File::create(vendor_dir.join("OWNERS"))?;
        writeln!(f, "# This file has been auto-generated by the `gnrt` tool.")?;
        writeln!(f, "file://{}", build_dir_owners_file.display())?;
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
}

fn get_placeholder_crate_metadata<'a>(
    package: guppy::graph::PackageMetadata<'a>,
) -> PlaceholderCrate<'a> {
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
    let mut dependencies: Vec<_> = package
        .direct_links()
        .filter(|link| !link.dev_only())
        .map(|link| PlaceholderDependency {
            name: link.to().name(),
            version: link.version_req().to_string(),
        })
        .collect();
    dependencies.sort_unstable_by(|left, right| left.name.cmp(right.name));

    let mut features: Vec<_> = package.named_features().map(str::to_string).collect();
    features.sort_unstable();

    PlaceholderCrate { name: package.name(), version: package.version(), dependencies, features }
}

fn generate_placeholder_crate(
    package: guppy::graph::PackageMetadata<'_>,
    crate_dir: &Path,
) -> Result<()> {
    const CARGO_TOML_TEMPLATE: &str = "`gnrt`-built-in `removed_Cargo.toml.hbs` template";
    const LIB_RS_TEMPLATE: &str = "`gnrt`-built-in `removed_lib.rs.hbs` template";
    let mut handlebars = init_handlebars();
    handlebars
        .register_template_string(CARGO_TOML_TEMPLATE, include_str!("removed_Cargo.toml.hbs"))?;
    handlebars.register_template_string(LIB_RS_TEMPLATE, include_str!("removed_lib.rs.hbs"))?;

    create_dirs_if_needed(crate_dir).context("creating crate dir")?;
    for x in std::fs::read_dir(crate_dir)? {
        let entry = x?;
        if entry.metadata()?.is_dir() {
            std::fs::remove_dir_all(entry.path())
                .with_context(|| format!("removing dir {}", entry.path().display()))?;
        } else {
            std::fs::remove_file(entry.path())
                .with_context(|| format!("removing file {}", entry.path().display()))?;
        }
    }

    let placeholder_crate = get_placeholder_crate_metadata(package);

    render_handlebars_named_template(
        &handlebars,
        CARGO_TOML_TEMPLATE,
        &placeholder_crate,
        &crate_dir.join("Cargo.toml"),
    )?;

    create_dirs_if_needed(&crate_dir.join("src")).context("creating src dir")?;
    render_handlebars_named_template(
        &handlebars,
        LIB_RS_TEMPLATE,
        &placeholder_crate,
        &crate_dir.join("src").join("lib.rs"),
    )?;

    std::fs::write(crate_dir.join(".cargo-checksum.json"), "{\"files\":{}}\n")
        .with_context(|| format!("writing .cargo-checksum.json for crate {}", package.name()))?;

    Ok(())
}

fn parse_cargo_toml_file(file: &Path) -> Result<toml::Table> {
    // Using manual, non-strongly-typed TOML parsing (instead of going through
    // `cargo metadata` or `guppy`) to work even if `Cargo.lock` is absent
    // (in this situation `cargo --locked --offline` would complain).
    let file_contents = std::fs::read_to_string(file)?;
    let toml_table = file_contents.parse::<toml::value::Table>()?;
    Ok(toml_table)
}

/// Parses `dir/Cargo.toml` to extract package name and version.
///
/// This is intended to be used during the vendoring process, to determine
/// if an existing `third_party/rust/chromium_crates_io/vendor/foo` directory
/// contains an up-to-date version of a crate.
fn get_package_id_from_vendored_dir(dir: &Path) -> Option<deps::PackageId> {
    let toml = parse_cargo_toml_file(&dir.join("Cargo.toml")).ok()?;
    let package = toml.get("package")?.as_table()?;
    let name = package.get("name")?.as_str()?;
    let version = package.get("version")?.as_str()?.parse().ok()?;
    Some(deps::PackageId::new(name.to_string(), version))
}

/// Checks if `dir` contains a "placeholder" crate (one crated by `fn
/// generate_placeholder_crate`).
fn is_placeholder_crate(dir: &Path) -> bool {
    fn try_is_placeholder_crate(dir: &Path) -> Option<bool> {
        let toml = parse_cargo_toml_file(&dir.join("Cargo.toml")).ok()?;
        let package = toml.get("package")?.as_table()?;
        let metadata = package.get("metadata")?.as_table()?;
        let gnrt_metadata = metadata.get("gnrt")?.as_table()?;
        gnrt_metadata.get("is_placeholder")?.as_bool()
    }
    try_is_placeholder_crate(dir).unwrap_or(false)
}

#[cfg(test)]
mod test {
    use super::*;

    use anyhow::anyhow;
    use semver::Version;
    use tempfile::TempDir;

    fn write_placeholder_crate_for_tests(
        cargo_metadata: &str,
        package_name: &str,
        path: &Path,
    ) -> Result<()> {
        let graph = guppy::CargoMetadata::parse_json(cargo_metadata)?.build_graph()?;
        let package = graph
            .resolve_package_name(package_name)
            .packages(guppy::graph::DependencyDirection::Forward)
            .exactly_one()
            .map_err(|_err| anyhow!("Expected exactly 1 package named `{package_name}`"))?;
        generate_placeholder_crate(package, path)
    }

    #[test]
    fn test_placeholder_crate_detection() {
        let temp_dir = TempDir::with_prefix("gnrt_unittests").unwrap();
        let crate_dir = temp_dir.path();
        assert!(!is_placeholder_crate(crate_dir));
        assert_eq!(None, get_package_id_from_vendored_dir(crate_dir));

        write_placeholder_crate_for_tests(SAMPLE_CARGO_METADATA2, "quote", crate_dir).unwrap();

        // Check that `get_package_id_from_vendored_dir` can detect the crate name and
        // version and that `is_placeholder_crate` returns true now.
        assert!(is_placeholder_crate(crate_dir));
        let Some(package_id) = get_package_id_from_vendored_dir(crate_dir) else {
            panic!("`None` returned from get_package_id_from_vendored_dir");
        };
        assert_eq!(package_id.name(), "quote");
        assert_eq!(*package_id.version(), Version::new(1, 0, 39));

        // Check that `cargo` can parse the generated `Cargo.toml`.
        let placeholder_graph = guppy::MetadataCommand::new()
            .manifest_path(&crate_dir.join("Cargo.toml"))
            .no_deps()
            .build_graph()
            .unwrap();
        let placeholder_package =
            placeholder_graph.packages().exactly_one().map_err(|_| ()).unwrap();
        assert_eq!(placeholder_package.name(), "quote");
        assert_eq!(*placeholder_package.version(), Version::new(1, 0, 39));
        let placeholder_features = placeholder_package
            .to_feature_set(guppy::graph::feature::StandardFeatures::All)
            .features(guppy::graph::DependencyDirection::Forward)
            .filter_map(|feature| {
                use guppy::graph::feature::FeatureLabel;
                match feature.label() {
                    FeatureLabel::Base | FeatureLabel::OptionalDependency(_) => None,
                    FeatureLabel::Named(feature_name) => Some(feature_name),
                }
            })
            .collect_vec();
        assert_eq!(placeholder_features, &["default", "proc-macro"]);
    }

    #[test]
    fn test_get_placeholder_crate_metadata_with_proc_macro2_dependency() {
        let graph: guppy::graph::PackageGraph =
            guppy::CargoMetadata::parse_json(SAMPLE_CARGO_METADATA2)
                .unwrap()
                .build_graph()
                .unwrap();
        let yoke_derive = graph.packages().find(|p| p.name() == "yoke-derive").unwrap();
        let placeholder = get_placeholder_crate_metadata(yoke_derive);
        assert_eq!(placeholder.name, "yoke-derive");
        assert_eq!(placeholder.version.to_string(), "0.8.0");

        let mut i = 0;
        assert_eq!(placeholder.dependencies[i].name, "proc-macro2");
        assert_eq!(placeholder.dependencies[i].version, "^1.0.61");

        i += 1;
        assert_eq!(placeholder.dependencies[i].name, "quote");
        assert_eq!(placeholder.dependencies[i].version, "^1.0.28");

        i += 1;
        assert_eq!(placeholder.dependencies[i].name, "syn");
        assert_eq!(placeholder.dependencies[i].version, "^2.0.21");

        i += 1;
        assert_eq!(placeholder.dependencies[i].name, "synstructure");
        assert_eq!(placeholder.dependencies[i].version, "^0.13.0");

        i += 1;
        assert_eq!(placeholder.dependencies.len(), i);
    }

    // `test_metadata2.json` contains the output of `cargo metadata` run in
    // `gnrt/sample_package2` directory.  See the `Cargo.toml` for more
    // information.
    static SAMPLE_CARGO_METADATA2: &str = include_str!("lib/test_metadata2.json");
}
