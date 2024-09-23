// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::config;
use crate::crates::{self, CrateFiles, Epoch, NormalizedName, VendoredCrate};
use crate::deps;
use crate::gn;
use crate::paths;
use crate::util::{
    check_exit_ok, check_spawn, check_wait_with_output, create_dirs_if_needed, init_handlebars,
    run_cargo_metadata,
};
use crate::GenCommandArgs;

use std::collections::{HashMap, HashSet};
use std::fs;
use std::io::{self, Write};
use std::path::{Path, PathBuf};
use std::process;

use anyhow::{ensure, format_err, Context, Result};

pub fn generate(args: GenCommandArgs, paths: &paths::ChromiumPaths) -> Result<()> {
    if args.for_std.is_some() {
        generate_for_std(args, paths)
    } else {
        generate_for_third_party(args, paths)
    }
}

fn generate_for_std(args: GenCommandArgs, paths: &paths::ChromiumPaths) -> Result<()> {
    // Load config file, which applies rustenv and cfg flags to some std crates.
    let config_file_contents = std::fs::read_to_string(paths.std_config_file).unwrap();
    let config: config::BuildConfig = toml::de::from_str(&config_file_contents).unwrap();

    let template_path =
        paths.std_config_file.parent().unwrap().join(&config.gn_config.build_file_template);

    let handlebars = init_handlebars(&template_path)?;

    // The Rust source tree, containing the standard library and vendored
    // dependencies.
    let rust_src_root = args.for_std.as_ref().unwrap();

    println!("Generating stdlib GN rules from {}", rust_src_root);

    let cargo_config = std::fs::read_to_string(paths.std_fake_root_config_template)
        .unwrap()
        .replace("RUST_SRC_ROOT", rust_src_root);
    std::fs::write(
        paths.strip_template(paths.std_fake_root_config_template).unwrap(),
        cargo_config,
    )
    .unwrap();

    let cargo_toml = std::fs::read_to_string(paths.std_fake_root_cargo_template)
        .unwrap()
        .replace("RUST_SRC_ROOT", rust_src_root);
    std::fs::write(paths.strip_template(paths.std_fake_root_cargo_template).unwrap(), cargo_toml)
        .unwrap();
    // Convert the `rust_src_root` to a Path hereafter.
    let rust_src_root = paths.root.join(Path::new(rust_src_root));

    // Delete the Cargo.lock if it exists.
    let mut std_fake_root_cargo_lock = paths.std_fake_root.to_path_buf();
    std_fake_root_cargo_lock.push("Cargo.lock");
    if let Err(e) = std::fs::remove_file(std_fake_root_cargo_lock) {
        match e.kind() {
            // Ignore if it already doesn't exist.
            std::io::ErrorKind::NotFound => (),
            _ => panic!("io error while deleting Cargo.lock: {e}"),
        }
    }

    // The Cargo.toml files in the Rust toolchain may use nightly Cargo
    // features, but the cargo binary is beta. This env var enables the
    // beta cargo binary to allow nightly features anyway.
    // https://github.com/rust-lang/rust/commit/2e52f4deb0544480b6aefe2c0cc1e6f3c893b081
    let cargo_extra_env: HashMap<std::ffi::OsString, std::ffi::OsString> =
        [("RUSTC_BOOTSTRAP".into(), "1".into())].into_iter().collect();

    // Use offline to constrain dependency resolution to those in the Rust src
    // tree and vendored crates. Ideally, we'd use "--locked" and use the
    // upstream Cargo.lock, but this is not straightforward since the rust-src
    // component is not a full Cargo workspace. Since the vendor dir we package
    // is generated with "--locked", the outcome should be the same.
    let cargo_extra_options = vec!["--offline".to_string()];

    // Compute the set of crates we need to build libstd. Note this
    // contains a few kinds of entries:
    // * Rust workspace packages (e.g. core, alloc, std, unwind, etc)
    // * Non-workspace packages supplied in Rust source tree (e.g. stdarch)
    // * Vendored third-party crates (e.g. compiler_builtins, libc, etc)
    // * rust-std-workspace-* shim packages which direct std crates.io
    //   dependencies to the correct lib{core,alloc,std} when depended on by the
    //   Rust codebase (see
    //   https://github.com/rust-lang/rust/tree/master/library/rustc-std-workspace-core)
    let mut dependencies = {
        let metadata =
            run_cargo_metadata(paths.std_fake_root.into(), cargo_extra_options, cargo_extra_env)
                .with_context(|| {
                    format!(
                        "Failed to parse cargo metadata in a directory synthesized from \
                         {} and {}",
                        paths.std_fake_root_cargo_template.display(),
                        paths.std_fake_root_config_template.display(),
                    )
                })?;
        deps::collect_dependencies(
            &metadata,
            Some(vec![config.resolve.root.clone()]),
            None,
            &config,
        )
    };

    // Filter out any crates' dependencies removed by config file.
    for dep in dependencies.iter_mut() {
        let combined: HashSet<&str> =
            config.get_combined_set(&dep.package_name, |crate_cfg| &crate_cfg.remove_deps);
        if combined.is_empty() {
            continue;
        }

        for kind in [&mut dep.dependencies, &mut dep.build_dependencies] {
            kind.retain(|dep_of_dep| !combined.contains(dep_of_dep.package_name.as_str()));
        }
    }

    // Remove any excluded dep entries.
    dependencies
        .retain(|dep| !config.resolve.remove_crates.iter().any(|r| **r == dep.package_name));

    // Remove dev dependencies since tests aren't run. Also remove build deps
    // since we configure flags and env vars manually. Include the root
    // explicitly since it doesn't get a dependency_kinds entry.
    dependencies.retain(|dep| dep.dependency_kinds.contains_key(&deps::DependencyKind::Normal));

    dependencies.sort_unstable_by(|a, b| {
        a.package_name.cmp(&b.package_name).then(a.version.cmp(&b.version))
    });
    for dep in dependencies.iter_mut() {
        // Rehome stdlib deps from the `rust_src_root` to where they will be installed
        // in the Chromium checkout.
        let gn_prefix = paths.root.join(paths.rust_src_installed);
        if let Some(lib) = dep.lib_target.as_mut() {
            ensure!(
                lib.root.canonicalize().unwrap().starts_with(&rust_src_root),
                "Found dependency that was not locally available: {} {}\n{:?}",
                dep.package_name,
                dep.version,
                dep
            );

            if let Ok(remain) = lib.root.canonicalize().unwrap().strip_prefix(&rust_src_root) {
                lib.root = gn_prefix.join(remain);
            }
        }

        if let Some(path) = dep.build_script.as_mut() {
            if let Ok(remain) = path.canonicalize().unwrap().strip_prefix(&rust_src_root) {
                *path = gn_prefix.join(remain);
            }
        }
    }

    let third_party_deps = dependencies.iter().filter(|dep| !dep.is_local).collect::<Vec<_>>();

    // Check that all resolved third party deps are available. First, collect
    // the set of third-party dependencies vendored in the Rust source package.
    let vendored_crates: HashSet<VendoredCrate> =
        crates::collect_std_vendored_crates(&rust_src_root.join(paths.rust_src_vendor_subdir))
            .unwrap()
            .into_iter()
            .collect();

    // Collect vendored dependencies, and also check that all resolved
    // dependencies point to our Rust source package. Build rules will be
    // generated for these crates separately from std, alloc, and core which
    // need special treatment.
    for dep in third_party_deps.iter() {
        // Only process deps with a library target: we are producing build rules
        // for the standard library, so transitive binary dependencies don't
        // make sense.
        if dep.lib_target.is_none() {
            continue;
        }

        vendored_crates
            .get(&VendoredCrate { name: dep.package_name.clone(), version: dep.version.clone() })
            .ok_or_else(|| {
                format_err!(
                    "Resolved dependency does not match any vendored crate: {} {}",
                    dep.package_name,
                    dep.version
                )
            })?;
    }

    let crate_inputs: HashMap<VendoredCrate, CrateFiles> = dependencies
        .iter()
        .filter(|p| p.lib_target.is_some())
        .map(|p| {
            crates::collect_crate_files(p, &config, crates::IncludeCrateTargets::LibOnly)
                .expect("missing a stdlib input file, did you gclient sync?")
        })
        .collect();

    let build_file = gn::build_file_from_deps(
        dependencies.iter(),
        paths,
        &config,
        gn::NameLibStyle::PackageName,
        |crate_id| crate_inputs.get(crate_id).unwrap(),
    )?;

    if args.dump_template_input {
        return serde_json::to_writer_pretty(
            std::fs::File::create("gnrt-template-input.json").context("opening dump file")?,
            &build_file,
        )
        .context("dumping gn information");
    }

    let gn_str = handlebars.render("template", &build_file)?;
    write_build_file(&paths.std_build.join("BUILD.gn"), gn_str).unwrap();

    Ok(())
}

fn generate_for_third_party(args: GenCommandArgs, paths: &paths::ChromiumPaths) -> Result<()> {
    let config_file_contents = std::fs::read_to_string(paths.third_party_config_file).unwrap();
    let config: config::BuildConfig = toml::de::from_str(&config_file_contents).unwrap();

    let template_path =
        paths.third_party_config_file.parent().unwrap().join(&config.gn_config.build_file_template);
    let handlebars = init_handlebars(&template_path)?;

    println!("Generating third-party GN rules from {}", paths.third_party_cargo_root.display());

    let cargo_extra_options = vec![
        // Use offline to constrain dependency resolution to locally vendored crates.
        "--offline".to_string(),
        // Use locked to prevent updating dependencies at the same time as generating
        // metadata.
        "--locked".to_string(),
    ];

    // Compute the set of all third-party crates.
    let mut dependencies = deps::collect_dependencies(
        &run_cargo_metadata(
            paths.third_party_cargo_root.into(),
            cargo_extra_options,
            HashMap::new(),
        )?,
        Some(vec![config.resolve.root.clone()]),
        None,
        &config,
    );

    // Filter out any crates' dependencies removed by config file.
    for dep in dependencies.iter_mut() {
        let all: Option<&Vec<String>> = Some(&config.all_config.remove_deps);
        let per: Option<&Vec<String>> =
            config.per_crate_config.get(&dep.package_name).map(|config| &config.remove_deps);

        let combined: Vec<&String> = all.into_iter().chain(per).flatten().collect();
        if combined.is_empty() {
            continue;
        }

        for kind in [&mut dep.dependencies, &mut dep.build_dependencies] {
            kind.retain(|dep_of_dep| !combined.iter().any(|r| **r == dep_of_dep.package_name));
        }
    }

    // Remove any excluded dep entries.
    dependencies
        .retain(|dep| !config.resolve.remove_crates.iter().any(|r| **r == dep.package_name));

    // Remove dev dependencies since tests aren't run.
    dependencies.retain(|dep| {
        dep.dependency_kinds.contains_key(&deps::DependencyKind::Normal)
        // TODO: Needed?
            || dep.dependency_kinds.contains_key(&deps::DependencyKind::Build)
    });

    dependencies.sort_unstable_by(|a, b| {
        a.package_name.cmp(&b.package_name).then(a.version.cmp(&b.version))
    });

    let crate_inputs: HashMap<VendoredCrate, CrateFiles> = dependencies
        .iter()
        .map(|p| {
            crates::collect_crate_files(p, &config, crates::IncludeCrateTargets::LibAndBin)
                .unwrap_or_else(|e| {
                    panic!(
                        "missing a crate input file for '{}'. Dependencies are not vendored?\n\
                         note: {}",
                        p.package_name, e
                    )
                })
        })
        .collect();

    // If there are multiple crates with the same epoch, this is unexpected.
    // Bail out.
    {
        let mut found = HashSet::new();
        for dep in &dependencies {
            let epoch = crates::Epoch::from_version(&dep.version);
            if !found.insert((&dep.package_name, epoch)) {
                Err(format_err!(
                    "Two '{}' crates found with the same {} epoch",
                    dep.package_name,
                    epoch
                ))?
            }
        }
    }

    // Split up the dependencies by crate and epoch.
    let all_build_files: HashMap<PathBuf, gn::BuildFile> = {
        let mut map = HashMap::new();
        for dep in &dependencies {
            let build_file = gn::build_file_from_deps(
                std::iter::once(dep),
                paths,
                &config,
                // TODO(danakj): Change to PackageName for consistency?
                gn::NameLibStyle::LibLiteral,
                |crate_id| crate_inputs.get(crate_id).unwrap(),
            )?;
            let path = paths
                .third_party
                .join(NormalizedName::from_crate_name(&dep.package_name).as_str())
                .join(Epoch::from_version(&dep.version).to_string());
            let previous = map.insert(path, build_file);
            if previous.is_some() {
                Err(format_err!(
                    "multiple versions of crate {} with the same epoch",
                    dep.package_name
                ))?
            }
        }
        map
    };

    for dir in all_build_files.keys() {
        create_dirs_if_needed(dir).context(format!("dir: {}", dir.display()))?;
    }

    if args.dump_template_input {
        for (dir, build_file) in &all_build_files {
            serde_json::to_writer_pretty(
                std::fs::File::create(dir.join("gnrt-template-input.json"))
                    .context("opening dump file")?,
                &build_file,
            )
            .context("dumping gn information")?;
        }
        return Ok(());
    }

    for (dir, build_file) in &all_build_files {
        let gn_str = handlebars.render("template", &build_file)?;
        write_build_file(&dir.join("BUILD.gn"), gn_str).unwrap();
    }
    Ok(())
}

fn write_build_file(path: &Path, content: String) -> Result<()> {
    let cmd_name = "gn format";
    let output_handle = fs::File::create(path)
        .with_context(|| format!("Could not create GN output file {}", path.to_string_lossy()))?;

    // Spawn a child process to format GN rules. The formatted GN is written to
    // the file `output_handle`.
    let mut child = check_spawn(
        process::Command::new(if cfg!(windows) { "gn.bat" } else { "gn" })
            .arg("format")
            .arg("--stdin")
            .stdin(process::Stdio::piped())
            .stdout(output_handle),
        cmd_name,
    )?;

    write!(io::BufWriter::new(child.stdin.take().unwrap()), "{}", content)
        .context("Failed to write to GN format process")?;
    check_exit_ok(&check_wait_with_output(child, cmd_name)?, cmd_name)
}
