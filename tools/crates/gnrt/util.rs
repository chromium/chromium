// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::paths::ChromiumPaths;

use anyhow::{ensure, format_err, Context, Result};
use handlebars::{handlebars_helper, Renderable};
use serde::Serialize;
use std::{
    borrow::Cow,
    collections::HashMap,
    fmt::Write,
    fs,
    path::{Path, PathBuf},
    process,
};

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
        write!(msg, " stdout:\n\n{}", String::from_utf8_lossy(&output.stdout)).unwrap();
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

/// Same as `run_cargo_metadata` but built on top of `guppy`.
pub fn get_guppy_package_graph(
    workspace_path: PathBuf,
    mut extra_options: Vec<String>,
    extra_env: HashMap<std::ffi::OsString, std::ffi::OsString>,
) -> Result<guppy::graph::PackageGraph> {
    // See the `[dependencies.cxxbridge-cmd]` section in
    // `third_party/rust/chromium_crates_io/Cargo.toml` for explanation why
    // `-Zbindeps` flag is needed.
    extra_options.push("-Zbindeps".to_string());

    let mut command = guppy::MetadataCommand::new();
    command.current_dir(workspace_path);
    command.other_options(extra_options);
    for (k, v) in extra_env.into_iter() {
        command.env(k, v);
    }

    log::debug!("invoking cargo with:\n`{:?}`", command.cargo_command());
    command.build_graph().context("running cargo metadata")
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

    log::debug!("invoking cargo {subcommand}");
    let mut handle = command.spawn().with_context(|| format!("running cargo {subcommand}"))?;
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

struct IfKeyPresentHelper();

impl handlebars::HelperDef for IfKeyPresentHelper {
    fn call<'reg: 'rc, 'rc>(
        &self,
        h: &handlebars::Helper<'rc>,
        r: &'reg handlebars::Handlebars<'reg>,
        ctx: &'rc handlebars::Context,
        rc: &mut handlebars::RenderContext<'reg, 'rc>,
        out: &mut dyn handlebars::Output,
    ) -> handlebars::HelperResult {
        let param_not_found =
            |idx| handlebars::RenderErrorReason::ParamNotFoundForIndex("is_key_present", idx);
        let key = h.param(0).and_then(|v| v.value().as_str()).ok_or(param_not_found(0))?;
        let dict = h.param(1).and_then(|v| v.value().as_object()).ok_or(param_not_found(1))?;
        let template = if dict.contains_key(key) { h.template() } else { h.inverse() };
        match template {
            None => Ok(()),
            Some(t) => t.render(r, ctx, rc, out),
        }
    }
}

pub fn init_handlebars<'reg>() -> handlebars::Handlebars<'reg> {
    let mut handlebars = handlebars::Handlebars::new();
    handlebars.set_strict_mode(true);

    // Don't escape output strings; the default is to escape for HTML output. Do
    // not auto-escape for GN either, so that non-string GN may also be passed.
    handlebars.register_escape_fn(handlebars::no_escape);

    // Install helper to escape inputs pasted in GN `".."` strings.
    handlebars_helper!(gn_escape: |x: String| escape_for_handlebars(&x));
    handlebars.register_helper("gn_escape", Box::new(gn_escape));

    // Install helper to detect presence of dictionary keys (which works even if
    // the corresponding value is "false-y / non-truth-y" - i.e. the helper
    // distinguishes "missing" / "none" VS `false` / `0` / empty-string, etc.).
    handlebars.register_helper("if_key_present", Box::new(IfKeyPresentHelper()));

    handlebars
}

fn template_path_to_registration_name(template_path: &Path) -> String {
    let filename = template_path
        .file_name()
        .map(|filename| filename.to_string_lossy())
        .unwrap_or(Cow::Borrowed("???no-filename???"));
    let hash = {
        use std::hash::{Hash, Hasher};
        let mut h = std::hash::DefaultHasher::new();
        template_path.hash(&mut h);
        h.finish()
    };
    format!("{filename}#{hash:#x}")
}

pub fn init_handlebars_with_template_paths<'a>(
    template_paths: &[&'a Path],
) -> Result<handlebars::Handlebars<'a>> {
    let mut handlebars = init_handlebars();
    for path in template_paths.iter() {
        // Explicitly check `path.exists()` to get a better error message, even though
        // TOCTOU means that we may still get an error below.
        ensure!(path.exists(), "File doesn't exist: {}", path.display());

        let template_name = template_path_to_registration_name(path);
        handlebars
            .register_template_file(&template_name, path)
            .with_context(|| format!("Loading handlebars template: {}", path.display()))?;
    }
    Ok(handlebars)
}

pub fn render_handlebars(
    handlebars: &handlebars::Handlebars,
    template_path: &Path,
    data: &impl Serialize,
    output_path: &Path,
) -> Result<()> {
    render_handlebars_named_template(
        handlebars,
        &template_path_to_registration_name(template_path),
        data,
        output_path,
    )
    .with_context(|| format!("Expanding handlebars template `{}`", template_path.display(),))
}

pub fn render_handlebars_named_template(
    handlebars: &handlebars::Handlebars,
    template_name: &str,
    data: &impl Serialize,
    output_path: &Path,
) -> Result<()> {
    std::fs::File::create(output_path)
        .map_err(anyhow::Error::new)
        .and_then(|output_file| {
            let buffered_output_file = std::io::BufWriter::new(output_file);
            handlebars.render_to_write(template_name, data, buffered_output_file)?;
            Ok(())
        })
        .with_context(|| format!("Expanding handlebars template into `{}`", output_path.display(),))
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
    fn test_string_excaping() {
        assert_eq!("foo bar", format!("{}", escape_for_handlebars("foo bar")));
        assert_eq!("foo bar ", format!("{}", escape_for_handlebars("foo\nbar\n")));
        assert_eq!(r#"foo \"bar\""#, format!("{}", escape_for_handlebars(r#"foo "bar""#)));
        assert_eq!("foo 'bar'", format!("{}", escape_for_handlebars("foo 'bar'")));
    }

    #[test]
    fn test_handlebars_helper_is_key_present() {
        fn render(data: serde_json::Value) -> String {
            let mut h = init_handlebars();
            h.register_template_string(
                "template",
                r#"
                    {{#if_key_present "foo" dict}}
                        true
                    {{else}}
                        false
                    {{/if_key_present}}
                "#,
            )
            .unwrap();
            h.render("template", &data).unwrap().trim().to_string()
        }

        assert_eq!("true", render(serde_json::json!({ "dict": { "foo": 456 }})));
        assert_eq!("false", render(serde_json::json!({ "dict": { "bar": 456 }})));
    }
}
