// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use gnrt_lib::*;

use crates::{ChromiumVendoredCrate, StdVendoredCrate};
use manifest::*;

use std::collections::{HashMap, HashSet};
use std::fs;
use std::io::{self, Write};
use std::path::{Path, PathBuf};
use std::process::{self, ExitCode};

use clap::arg;

fn main() -> ExitCode {
    let args = clap::Command::new("gnrt")
        .subcommand(clap::Command::new("gen")
            .about("Generate GN build rules from third_party/rust crates")
            .arg(arg!(--"output-cargo-toml" "Output third_party/rust/Cargo.toml then exit \
                immediately"))
            .arg(arg!(--"skip-patch" "Don't apply gnrt_build_patch after generating build files. \
                Useful when updating the patch."))
            .arg(arg!(--"for-std" "(WIP) Generate build files for Rust std library instead of \
                third_party/rust"))
        )
        .subcommand(clap::Command::new("download")
            .about("Download the crate with the given name and version to third_party/rust.")
            .arg(arg!([NAME] "Name of the crate to download").required(true))
            .arg(arg!([VERSION] "Version of the crate to download").required(true))
            .arg(
                arg!(--"security-critical" <YESNO> "Whether the crate is considered to be \
                    security critical."
                ).possible_values(["yes", "no"]).required(true)
            )
        )
        .get_matches();

    let paths = paths::ChromiumPaths::new().unwrap();

    match args.subcommand() {
        Some(("gen", args)) => {
            if args.is_present("for-std") {
                // This is not fully implemented. Currently, it will print data helpful
                // for development then quit.
                generate_for_std(&args, &paths)
            } else {
                generate_for_third_party(&args, &paths)
            }
        }
        Some(("download", args)) => {
            let security = args.value_of("security-critical").unwrap() == "yes";
            let name = args.value_of("NAME").unwrap();
            use std::str::FromStr;
            let version = semver::Version::from_str(args.value_of("VERSION").unwrap())
                .expect("Invalid version specified");
            download::download(name, version, security, &paths)
        }
        _ => unreachable!("Invalid subcommand"),
    }
}

fn generate_for_third_party(args: &clap::ArgMatches, paths: &paths::ChromiumPaths) -> ExitCode {
    let manifest_contents =
        String::from_utf8(fs::read(paths.third_party.join("third_party.toml")).unwrap()).unwrap();
    let mut third_party_manifest: ThirdPartyManifest = match toml::de::from_str(&manifest_contents)
    {
        Ok(m) => m,
        Err(e) => {
            eprintln!("Failed to parse 'third_party.toml': {e}");
            return ExitCode::FAILURE;
        }
    };

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

    if args.is_present("output-cargo-toml") {
        println!("{}", toml::ser::to_string(&cargo_manifest).unwrap());
        return ExitCode::SUCCESS;
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
    let dependencies = deps::collect_dependencies(&command.exec().unwrap(), None);

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

    if has_error {
        return ExitCode::FAILURE;
    }

    let build_files: HashMap<ChromiumVendoredCrate, gn::BuildFile> = gn::build_files_from_deps(
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

    if has_error {
        return ExitCode::FAILURE;
    }

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

    // Apply patch for BUILD.gn files.
    let build_patch = paths.root.join(paths.third_party.join("gnrt_build_patch"));
    if !args.is_present("skip-patch") && build_patch.exists() {
        let status = process::Command::new("git")
            .arg("apply")
            .arg(build_patch)
            .current_dir(&paths.root)
            .status()
            .unwrap();
        check_exit_status(status, "applying build patch").unwrap();
    }

    ExitCode::SUCCESS
}

fn generate_for_std(_args: &clap::ArgMatches, paths: &paths::ChromiumPaths) -> ExitCode {
    // Run `cargo metadata` from the std package in the Rust source tree (which
    // is a workspace).
    let mut command = cargo_metadata::MetadataCommand::new();
    command.current_dir(paths.rust_std);
    // Ensure we use exactly the dependency versions specified in the Rust
    // source's Cargo.lock. This is the only officially tested libstd.
    command.other_options(["--locked".to_string(), "--offline".to_string()]);

    // Compute the set of crates we need to build to build libstd.
    let dependencies =
        deps::collect_dependencies(&command.exec().unwrap(), Some(vec!["std".to_string()]));

    // Collect the set of third-party dependencies vendored in the Rust source
    // package.
    let vendored_crates: HashMap<StdVendoredCrate, manifest::CargoPackage> =
        crates::collect_std_vendored_crates(paths.rust_src_vendor).unwrap().into_iter().collect();

    // Collect vendored dependencies, and also check that all resolved
    // dependencies point to our Rust source package. Build rules will be
    // generated for these crates separately from std, alloc, and core which
    // need special treatment.
    let src_prefix = paths.root.join(paths.rust_src);
    for dep in &dependencies {
        // Skip workspace members. They are not third-party deps in this
        // context.
        if dep.is_workspace_member {
            continue;
        }

        // Skip "rust-std-workspace-*" deps, which are shims to allow std to
        // depend on third-party crates. See
        // https://github.com/rust-lang/rust/tree/master/library/rustc-std-workspace-core
        if dep.package_name.starts_with("rustc-std-workspace-") {
            continue;
        }

        // Skip crates in stdarch, which is a separate workspace in the same
        // source tree.
        if dep.package_name.starts_with("std_detect") {
            continue;
        }

        // Skip "rustc-workspace-hack", which is irrelevant to the standard
        // library build.
        if dep.package_name.starts_with("rustc-workspace-hack") {
            continue;
        }

        // Only process deps with a library target: we are producing build rules
        // for the standard library, so transitive binary dependencies don't
        // make sense.
        let lib = match &dep.lib_target {
            Some(lib) => lib,
            None => continue,
        };

        if !lib.root.starts_with(&src_prefix) {
            println!(
                "Found dependency that was not locally available: {} {}",
                dep.package_name, dep.version
            );
            return ExitCode::FAILURE;
        }

        let (crate_id, _): (&StdVendoredCrate, _) =
            match vendored_crates.get_key_value(&StdVendoredCrate {
                name: dep.package_name.clone(),
                version: dep.version.clone(),
                // Placeholder value for lookup.
                is_latest: false,
            }) {
                Some(id) => id,
                None => {
                    println!(
                        "Resolved dependency does not match any vendored crate: {} {}",
                        dep.package_name, dep.version
                    );
                    return ExitCode::FAILURE;
                }
            };

        // To test, print the list of needed vendored crates and the path of
        // dependency edges leading to them.
        println!("{crate_id}");
        for next in &dep.dependency_path {
            println!("  {next}");
        }
    }

    ExitCode::SUCCESS
}

fn build_file_path(crate_id: &ChromiumVendoredCrate, paths: &paths::ChromiumPaths) -> PathBuf {
    let mut path = paths.root.clone();
    path.push(&paths.third_party);
    path.push(crate_id.build_path());
    path.push("BUILD.gn");
    path
}

fn write_build_file(path: &Path, build_file: &gn::BuildFile) -> io::Result<()> {
    let output_handle = fs::File::create(path).unwrap();

    // Run our GN output through the official formatter. The gn invocation will
    // write directly to the output file.
    let mut formatter = process::Command::new("gn")
        .arg("format")
        .arg("--stdin")
        .stdin(process::Stdio::piped())
        .stdout(output_handle)
        .spawn()
        .unwrap();

    write!(io::BufWriter::new(formatter.stdin.take().unwrap()), "{}", build_file.display())?;

    let status = formatter.wait()?;
    check_exit_status(status, "formatting GN output")
}

fn check_exit_status(status: process::ExitStatus, cmd_msg: &str) -> io::Result<()> {
    use std::fmt::Write;

    if status.success() {
        Ok(())
    } else {
        let mut msg: String = format!("{cmd_msg} failed with ");
        match status.code() {
            Some(code) => write!(msg, "{code}").unwrap(),
            None => write!(msg, "no code").unwrap(),
        };
        Err(io::Error::new(io::ErrorKind::Other, msg))
    }
}

fn create_dirs_if_needed(path: &Path) -> io::Result<()> {
    if path.is_dir() {
        return Ok(());
    }

    if let Some(parent) = path.parent() {
        create_dirs_if_needed(parent)?;
    }

    fs::create_dir(path)
}

/// A message prepended to autogenerated files. Notes this tool generated it and
/// not to edit directly.
static AUTOGENERATED_FILE_HEADER: &'static str = "!!! DO NOT EDIT -- Autogenerated by gnrt from third_party.toml. Edit that file instead. See tools/crates/gnrt.";
