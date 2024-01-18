// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::paths::ChromiumPaths;
use handlebars::handlebars_helper;
use std::collections::HashMap;
use std::fs;
use std::path::Path;
use std::process;
use std::{fmt::Write, path::PathBuf};

use anyhow::{format_err, Context, Result};

pub fn check_spawn(cmd: &mut process::Command, cmd_msg: &str) -> Result<process::Child> {
    cmd.spawn().with_context(|| format!("failed to start {cmd_msg}"))
}

pub fn check_wait_with_output(child: process::Child, cmd_msg: &str) -> Result<process::Output> {
    child.wait_with_output().with_context(|| format!("unexpected error while running {cmd_msg}"))
}

pub fn run_command(mut cmd: process::Command, cmd_msg: &str, stdin: Option<&[u8]>) -> Result<()> {
    if stdin.is_some() {
        cmd.stdin(std::process::Stdio::piped());
    }
    let mut child = check_spawn(&mut cmd, cmd_msg)?;
    if let Some(stdin) = stdin {
        use std::io::Write;
        child.stdin.as_mut().unwrap().write_all(stdin)?;
    }
    let status = child.wait()?;
    if !status.success() {
        Err(format_err!("command '{}' failed: {}", cmd_msg, status))
    } else {
        Ok(())
    }
}

pub fn check_exit_ok(output: &process::Output, cmd_msg: &str) -> Result<()> {
    if output.status.success() {
        Ok(())
    } else {
        let mut msg: String = format!("{cmd_msg} failed with ");
        match output.status.code() {
            Some(code) => write!(msg, "{code}.").unwrap(),
            None => write!(msg, "no code.").unwrap(),
        };
        write!(msg, " stderr:\n\n{}", String::from_utf8_lossy(&output.stderr)).unwrap();

        Err(format_err!(msg))
    }
}

pub fn create_dirs_if_needed(path: &Path) -> Result<()> {
    if path.is_dir() {
        return Ok(());
    }

    if let Some(parent) = path.parent() {
        create_dirs_if_needed(parent)?;
    }

    fs::create_dir(path)
        .with_context(|| format_err!("Could not create directories for {}", path.to_string_lossy()))
}

/// Runs a function with the `.cargo/config.toml` file removed for the duration
/// of the function. This allows access to the online crates.io repository
/// instead of using our vendor/ directory as the source of truth. It should
/// only be done for actions like adding or updating crates.
pub fn without_cargo_config_toml<T>(
    paths: &ChromiumPaths,
    f: impl FnOnce() -> Result<T>,
) -> Result<T> {
    let config_file = paths.third_party_cargo_root.join(".cargo").join("config.toml");
    let config_contents =
        std::fs::read_to_string(&config_file).context("reading .cargo/config.toml");
    if config_contents.is_ok() {
        std::fs::remove_file(&config_file)?;
    }

    let r = f();

    if let Ok(contents) = config_contents {
        std::fs::write(config_file, contents).context("writing .cargo/config.toml")?;
    }
    r
}

/// Run cargo metadata command, optionally with extra flags and environment.
pub fn run_cargo_metadata(
    workspace_path: PathBuf,
    mut extra_options: Vec<String>,
    extra_env: HashMap<std::ffi::OsString, std::ffi::OsString>,
) -> Result<cargo_metadata::Metadata> {
    let mut command = cargo_metadata::MetadataCommand::new();
    command.current_dir(workspace_path);

    // Allow the binary dependency on cxxbridge-cmd.
    extra_options.push("-Zbindeps".to_string());
    command.other_options(extra_options);
    for (k, v) in extra_env.into_iter() {
        command.env(k, v);
    }

    log::debug!("invoking cargo with:\n`{:?}`", command.cargo_command());
    command.exec().context("running cargo metadata")
}

/// Run a cargo command, other than metadata which should use
/// `run_cargo_metadata`.
pub fn run_cargo_command(
    workspace_path: PathBuf,
    subcommand: &str,
    extra_options: Vec<String>,
    extra_env: HashMap<std::ffi::OsString, std::ffi::OsString>,
) -> Result<()> {
    assert!(subcommand != "metadata");

    let mut command = std::process::Command::new("cargo");
    command.current_dir(workspace_path);

    // Allow the binary dependency on cxxbridge-cmd.
    command.arg("-Zbindeps");
    command.arg(subcommand);
    command.args(extra_options);

    for (k, v) in extra_env.into_iter() {
        command.env(k, v);
    }

    log::debug!("invoking cargo {}", subcommand);
    let mut handle = command.spawn().with_context(|| format!("running cargo {}", subcommand))?;
    let code = handle.wait().context("waiting for cargo process")?;
    if !code.success() {
        Err(format_err!("cargo {} exited with status {}", subcommand, code))
    } else {
        Ok(())
    }
}

pub fn remove_checksums_from_lock(cargo_root: &Path) -> Result<()> {
    let lock_file_path = cargo_root.join("Cargo.lock");
    let lock_contents = std::fs::read_to_string(&lock_file_path)?
        .lines()
        .filter(|line| !line.starts_with("checksum = "))
        .map(String::from)
        // Add (back) the trailing newline.
        .chain(std::iter::once(String::new()))
        .collect::<Vec<_>>();
    std::fs::write(&lock_file_path, lock_contents.join("\n"))?;
    Ok(())
}

pub fn init_handlebars(template_path: &Path) -> Result<handlebars::Handlebars> {
    let mut handlebars = handlebars::Handlebars::new();

    // Don't escape output strings; the default is to escape for HTML output. Do
    // not auto-escape for GN either, so that non-string GN may also be passed.
    handlebars.register_escape_fn(handlebars::no_escape);
    handlebars.register_template_file("template", template_path).context("loading gn template")?;

    // Install helper to escape inputs pasted in GN `".."` strings.
    handlebars_helper!(gn_escape: |x: String| escape_for_handlebars(&x));
    handlebars.register_helper("gn_escape", Box::new(gn_escape));
    Ok(handlebars)
}

fn escape_for_handlebars(x: &str) -> String {
    let mut out = String::new();
    for c in x.chars() {
        match c {
            // Note: we don't escape '$' here because we sometimes want to use
            // $var syntax.
            c @ ('"' | '\\') => write!(out, "\\{c}").unwrap(),
            // GN strings can encode literal ASCII with "$0x<hex_code>" syntax,
            // so we could embed newlines with "$0x0A". However, GN seems to
            // escape these incorrectly in its Ninja output so we just replace
            // it with a space.
            '\n' => out.push(' '),
            c => out.push(c),
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn string_excaping() {
        assert_eq!("foo bar", format!("{}", escape_for_handlebars("foo bar")));
        assert_eq!("foo bar ", format!("{}", escape_for_handlebars("foo\nbar\n")));
        assert_eq!(r#"foo \"bar\""#, format!("{}", escape_for_handlebars(r#"foo "bar""#)));
        assert_eq!("foo 'bar'", format!("{}", escape_for_handlebars("foo 'bar'")));
    }
}
