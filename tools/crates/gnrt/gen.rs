// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use gnrt_lib::*;

use crates::{ChromiumVendoredCrate, StdVendoredCrate};
use manifest::*;

use crate::util::{check_exit_ok, check_spawn, check_wait_with_output, create_dirs_if_needed};

use std::collections::{HashMap, HashSet};
use std::fs;
use std::io::{self, Write};
use std::path::{Path, PathBuf};
use std::process;

use anyhow::{ensure, format_err, Context, Result};

pub fn generate(args: &clap::ArgMatches, paths: &paths::ChromiumPaths) -> Result<()> {
    if args.get_flag("for-std") {
        // This is not fully implemented. Currently, it will print data helpful
        // for development then quit.
        generate_for_std(&args, &paths)
    } else {
        generate_for_third_party(&args, &paths)
    }
}

fn generate_for_third_party(args: &clap::ArgMatches, paths: &paths::ChromiumPaths) -> Result<()> {
    let manifest_contents =
        String::from_utf8(fs::read(paths.third_party.join("third_party.toml")).unwrap()).unwrap();
    let mut third_party_manifest: ThirdPartyManifest =
        toml::de::from_str(&manifest_contents).context("Could not parse third_party.toml")?;

    // Collect special fields from third_party.toml.
    //
    // TODO(crbug.com/1291994): handle visibility separately for each kind.
    let mut deps_visibility = HashMap::<ChromiumVendoredCrate, crates::Visibility>::new();
    let mut build_script_outputs = HashMap::<ChromiumVendoredCrate, Vec<String>>::new();
    let mut gn_variables_libs = HashMap::<ChromiumVendoredCrate, String>::new();

    let mut walk_deps = |dep_name: &str, dep_spec: &Dependency, visibility: crates::Visibility| {
        let (version_req, is_public, dep_outputs, gn_variables_lib): (
            &_,
            bool,
            &[_],
            Option<&String>,
        ) = match dep_spec {
            Dependency::Short(version_req) => (version_req, true, &[], None),
            Dependency::Full(dep) => (
                dep.version.as_ref().unwrap(),
                dep.allow_first_party_usage,
                &dep.build_script_outputs,
                dep.gn_variables_lib.as_ref(),
            ),
        };
        let epoch = crates::Epoch::from_version_req_str(&version_req.0);
        let crate_id = ChromiumVendoredCrate { name: dep_name.to_string(), epoch };
        deps_visibility.insert(
            crate_id.clone(),
            if is_public { visibility } else { crates::Visibility::ThirdParty },
        );
        if !dep_outputs.is_empty() {
            build_script_outputs.insert(crate_id.clone(), dep_outputs.to_vec());
        }
        if gn_variables_lib.is_some() {
            gn_variables_libs.insert(crate_id, gn_variables_lib.unwrap().clone());
        }
    };

    for (dep_name, dep_spec) in &third_party_manifest.dependency_spec.dev_dependencies {
        walk_deps(dep_name, dep_spec, crates::Visibility::TestOnlyAndThirdParty)
    }
    for (dep_name, dep_spec) in &third_party_manifest.dependency_spec.dependencies {
        walk_deps(dep_name, dep_spec, crates::Visibility::Public)
    }
    // [build-dependencies] is not used in third_party.toml.

    // For crates used in first-party tests, we do not build a separate library from
    // production (unlike standard Rust tests, and those found in third-party
    // crates.) So we merge the dev_dependencies from third_party.toml into the
    // regular dependencies.
    third_party_manifest
        .dependency_spec
        .dependencies
        .extend(std::mem::take(&mut third_party_manifest.dependency_spec.dev_dependencies));

    // Rebind as immutable.
    let (third_party_manifest, deps_visibility, build_script_outputs, gn_variables_libs) =
        (third_party_manifest, deps_visibility, build_script_outputs, gn_variables_libs);

    // Traverse our third-party directory to collect the set of vendored crates.
    // Used to generate Cargo.toml [patch] sections, and later to check against
    // `cargo metadata`'s dependency resolution to ensure we have all the crates
    // we need. We sort `crates` for a stable ordering of [patch] sections.
    let mut crates = crates::collect_third_party_crates(paths.third_party.clone()).unwrap();
    crates.sort_unstable_by(|a, b| a.0.cmp(&b.0));

    // Generate a fake root Cargo.toml for dependency resolution.
    let cargo_manifest = generate_fake_cargo_toml(
        third_party_manifest,
        crates.iter().map(|(c, _)| manifest::PatchSpecification {
            package_name: c.name.clone(),
            patch_name: c.patch_name(),
            path: c.crate_path(),
        }),
    );

    if args.get_flag("output-cargo-toml") {
        println!("{}", toml::ser::to_string(&cargo_manifest).unwrap());
        return Ok(());
    }

    // Create a fake package: Cargo.toml and an empty main.rs. This allows cargo
    // to construct a full dependency graph as if Chrome were a cargo package.
    write!(
        io::BufWriter::new(fs::File::create(paths.third_party.join("Cargo.toml")).unwrap()),
        "# {}\n\n{}",
        AUTOGENERATED_FILE_HEADER,
        toml::to_string(&cargo_manifest).unwrap()
    )
    .unwrap();
    create_dirs_if_needed(&paths.third_party.join("src")).unwrap();
    write!(
        io::BufWriter::new(fs::File::create(paths.third_party.join("src/main.rs")).unwrap()),
        "// {}",
        AUTOGENERATED_FILE_HEADER
    )
    .unwrap();

    // Run `cargo metadata` and process the output to get a list of crates we
    // depend on.
    let mut command = cargo_metadata::MetadataCommand::new();
    command.current_dir(&paths.third_party);
    let dependencies = deps::collect_dependencies(&command.exec().unwrap(), None, None);

    // Compare cargo's dependency resolution with the crates we have on disk. We
    // want to ensure:
    // * Each resolved dependency matches with a crate we discovered (no missing
    //   deps).
    // * Each discovered crate matches with a resolved dependency (no unused
    //   crates).
    let mut has_error = false;
    let present_crates: HashSet<&ChromiumVendoredCrate> = crates.iter().map(|(c, _)| c).collect();

    // Construct the set of requested third-party crates, ensuring we don't have
    // duplicate epochs. For example, if we resolved two versions of a
    // dependency with the same major version, we cannot continue.
    let mut req_crates = HashSet::<ChromiumVendoredCrate>::new();
    for package in &dependencies {
        if !req_crates.insert(package.third_party_crate_id()) {
            panic!("found another requested package with the same name and epoch: {:?}", package);
        }
    }
    let req_crates = req_crates;

    for dep in dependencies.iter() {
        if !present_crates.contains(&dep.third_party_crate_id()) {
            has_error = true;
            println!("Missing dependency: {} {}", dep.package_name, dep.version);
            for edge in dep.dependency_path.iter() {
                println!("    {edge}");
            }
        } else if !dep.is_local {
            // Transitive deps may be requested with version requirements stricter than
            // ours: e.g. 1.57 instead of just major version 1. If the version we have
            // checked in, e.g. 1.56, has the same epoch but doesn't meet the version
            // requirement, the symptom is Cargo will resolve the dependency to an
            // upstream source instead of our local path. We must detect this case to
            // ensure correctness.
            has_error = true;
            println!(
                "Resolved {} {} to an upstream source. The requested version \
                 likely has the same epoch as the discovered crate but \
                 something has a more stringent version requirement.",
                dep.package_name, dep.version
            );
            println!("    Resolved version: {}", dep.version);
        }
    }

    for present_crate in present_crates.iter() {
        if !req_crates.contains(present_crate) {
            has_error = true;
            println!("Unused crate: {present_crate}");
        }
    }

    ensure!(!has_error, "Dependency resolution failed");

    let build_files: HashMap<ChromiumVendoredCrate, gn::BuildFile> =
        gn::build_files_from_chromium_deps(
            &dependencies,
            &paths,
            &crates.iter().cloned().collect(),
            &build_script_outputs,
            &deps_visibility,
            &gn_variables_libs,
        );

    // Before modifying anything make sure we have a one-to-one mapping of
    // discovered crates and build file data.
    for (crate_id, _) in build_files.iter() {
        // This shouldn't happen, but check anyway in case we have a strange
        // logic error above.
        assert!(present_crates.contains(&crate_id));
    }

    for crate_id in present_crates.iter() {
        if !build_files.contains_key(*crate_id) {
            println!("Error: discovered crate {crate_id}, but no build file was generated.");
            has_error = true;
        }
    }

    ensure!(!has_error, "Generated build rules don't match input dependencies");

    // Wipe all previous BUILD.gn files. If we fail, we don't want to leave a
    // mix of old and new build files.
    for build_file in crates.iter().map(|(crate_id, _)| build_file_path(crate_id, &paths)) {
        if build_file.exists() {
            fs::remove_file(&build_file).unwrap();
        }
    }

    // Generate build files, wiping the previous ones so we don't have any stale
    // build rules.
    for (crate_id, _) in crates.iter() {
        let build_file_path = build_file_path(crate_id, &paths);
        let build_file_data = match build_files.get(&crate_id) {
            Some(build_file) => build_file,
            None => panic!("missing build data for {crate_id}"),
        };

        write_build_file(&build_file_path, build_file_data).unwrap();
    }

    Ok(())
}

fn generate_for_std(_args: &clap::ArgMatches, paths: &paths::ChromiumPaths) -> Result<()> {
    // Load config file, which applies rustenv and cfg flags to some std crates.
    let config_file_contents = std::fs::read_to_string(paths.std_config_file).unwrap();
    let config: config::BuildConfig = toml::de::from_str(&config_file_contents).unwrap();

    // Run `cargo metadata` from the std package in the Rust source tree (which
    // is a workspace).
    let mut command = cargo_metadata::MetadataCommand::new();
    command.current_dir(paths.std_fake_root);

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

    // Use offline to constrain dependency resolution to those in the Rust src
    // tree and vendored crates. Ideally, we'd use "--locked" and use the
    // upstream Cargo.lock, but this is not straightforward since the rust-src
    // component is not a full Cargo workspace. Since the vendor dir we package
    // is generated with "--locked", the outcome should be the same.
    command.other_options(vec!["--offline".to_string()]);

    // Compute the set of crates we need to build to build libstd. Note this
    // contains a few kinds of entries:
    // * Rust workspace packages (e.g. core, alloc, std, unwind, etc)
    // * Non-workspace packages supplied in Rust source tree (e.g. stdarch)
    // * Vendored third-party crates (e.g. compiler_builtins, libc, etc)
    // * rust-std-workspace-* shim packages which direct std crates.io
    //   dependencies to the correct lib{core,alloc,std} when depended on by the
    //   Rust codebase (see
    //   https://github.com/rust-lang/rust/tree/master/library/rustc-std-workspace-core)
    //
    // libtest is the root of the std crate dependency tree, so start there.
    let mut dependencies =
        deps::collect_dependencies(&command.exec().unwrap(), Some(vec!["test".to_string()]), None);

    // Remove dev dependencies since tests aren't run. Also remove build deps
    // since we configure flags and env vars manually. Include libtest
    // explicitly since, as the root of collect_dependencies(), it doesn't get a
    // dependency_kinds entry.
    dependencies.retain(|dep| {
        dep.package_name == "test"
            || dep.dependency_kinds.contains_key(&deps::DependencyKind::Normal)
    });

    dependencies.sort_unstable_by(|a, b| {
        a.package_name.cmp(&b.package_name).then(a.version.cmp(&b.version))
    });

    let third_party_deps = dependencies.iter().filter(|dep| !dep.is_local).collect::<Vec<_>>();

    // Check that all resolved third party deps are available. First, collect
    // the set of third-party dependencies vendored in the Rust source package.
    let vendored_crates: HashMap<StdVendoredCrate, manifest::CargoPackage> =
        crates::collect_std_vendored_crates(paths.rust_src_vendor).unwrap().into_iter().collect();

    // Collect vendored dependencies, and also check that all resolved
    // dependencies point to our Rust source package. Build rules will be
    // generated for these crates separately from std, alloc, and core which
    // need special treatment.
    let src_prefix = paths.root.join(paths.rust_src);
    for dep in third_party_deps.iter() {
        // Only process deps with a library target: we are producing build rules
        // for the standard library, so transitive binary dependencies don't
        // make sense.
        let lib = match &dep.lib_target {
            Some(lib) => lib,
            None => continue,
        };

        ensure!(
            lib.root.canonicalize().unwrap().starts_with(&src_prefix),
            "Found dependency that was not locally available: {} {}\n{:?}",
            dep.package_name,
            dep.version,
            dep
        );

        vendored_crates
            .get_key_value(&StdVendoredCrate {
                name: dep.package_name.clone(),
                version: dep.version.clone(),
                // Placeholder value for lookup.
                is_latest: false,
            })
            .ok_or_else(|| {
                format_err!(
                    "Resolved dependency does not match any vendored crate: {} {}",
                    dep.package_name,
                    dep.version
                )
            })?;
    }

    let build_file = gn::build_file_from_std_deps(dependencies.iter(), paths, &config);
    write_build_file(&paths.std_build.join("BUILD.gn"), &build_file).unwrap();

    Ok(())
}

fn build_file_path(crate_id: &ChromiumVendoredCrate, paths: &paths::ChromiumPaths) -> PathBuf {
    let mut path = paths.root.clone();
    path.push(&paths.third_party);
    path.push(crate_id.build_path());
    path.push("BUILD.gn");
    path
}

fn write_build_file(path: &Path, build_file: &gn::BuildFile) -> Result<()> {
    let cmd_name = "gn format";
    let output_handle = fs::File::create(path)
        .with_context(|| format!("Could not create GN output file {}", path.to_string_lossy()))?;

    // Spawn a child process to format GN rules. The formatted GN is written to
    // the file `output_handle`.
    let mut child = check_spawn(
        &mut process::Command::new("gn")
            .arg("format")
            .arg("--stdin")
            .stdin(process::Stdio::piped())
            .stdout(output_handle),
        cmd_name,
    )?;

    write!(io::BufWriter::new(child.stdin.take().unwrap()), "{}", build_file.display())
        .context("Failed to write to GN format process")?;
    check_exit_ok(&check_wait_with_output(child, cmd_name)?, cmd_name)
}

/// A message prepended to autogenerated files. Notes this tool generated it and
/// not to edit directly.
static AUTOGENERATED_FILE_HEADER: &'static str = "!!! DO NOT EDIT -- Autogenerated by gnrt from third_party.toml. Edit that file instead. See tools/crates/gnrt.";
