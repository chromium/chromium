// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::panic::UnwindSafe;

use crate::RebuildDependencyRecorder;
use autocxx_bindgen::callbacks::ParseCallbacks;

#[derive(Debug)]
pub(crate) struct AutocxxParseCallbacks(pub(crate) Box<dyn RebuildDependencyRecorder>);

impl UnwindSafe for AutocxxParseCallbacks {}

impl ParseCallbacks for AutocxxParseCallbacks {
    fn include_file(&self, filename: &str) {
        self.0.record_header_file_dependency(filename);
    }
}
