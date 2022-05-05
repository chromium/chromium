// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use proc_macro2::TokenStream;
use std::io::Write;
use std::process::{Command, Stdio};

enum Error {
    Run(std::io::Error),
    Write(std::io::Error),
    Utf8(std::string::FromUtf8Error),
    Wait(std::io::Error),
}

pub(crate) fn pretty_print(ts: &TokenStream) -> String {
    reformat_or_else(ts.to_string())
}

fn reformat_or_else(text: impl std::fmt::Display) -> String {
    match reformat(&text) {
        Ok(s) => s,
        Err(_) => text.to_string(),
    }
}

fn reformat(text: impl std::fmt::Display) -> Result<String, Error> {
    let mut rustfmt = Command::new("rustfmt")
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .spawn()
        .map_err(Error::Run)?;
    write!(rustfmt.stdin.take().unwrap(), "{}", text).map_err(Error::Write)?;
    let output = rustfmt.wait_with_output().map_err(Error::Wait)?;
    String::from_utf8(output.stdout).map_err(Error::Utf8)
}
