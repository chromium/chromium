// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::fmt::Write;
use std::fs;
use std::path::Path;
use std::process;

use anyhow::{format_err, Context, Result};

pub fn check_output(cmd: &mut process::Command, cmd_msg: &str) -> Result<process::Output> {
    cmd.output().with_context(|| format!("failed to start {cmd_msg}"))
}

pub fn check_spawn(cmd: &mut process::Command, cmd_msg: &str) -> Result<process::Child> {
    cmd.spawn().with_context(|| format!("failed to start {cmd_msg}"))
}

pub fn check_wait_with_output(child: process::Child, cmd_msg: &str) -> Result<process::Output> {
    child.wait_with_output().with_context(|| format!("unexpected error while running {cmd_msg}"))
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
