// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

use std::collections::HashSet;

/// Type which tracks the uniqueness of the _output_ names from
/// cxx (as dictated by the #[rust_name] attribute).
/// See also `bridge_name_tracker` which tracks
/// the names of the items in the cxx::bridge: there's a big comment
/// there explaining the relationship of all the names.
#[derive(Default)]
pub(crate) struct RustNameTracker {
    rust_names_used: HashSet<String>,
}

impl RustNameTracker {
    pub(crate) fn new() -> Self {
        Self::default()
    }

    /// Is it OK to create a global Rust function with this name
    /// in the output of the cxx module?
    pub(crate) fn ok_to_use_rust_name(&mut self, rust_name: &str) -> bool {
        self.rust_names_used.insert(rust_name.to_string())
    }
}

#[cfg(test)]
mod tests {
    use super::RustNameTracker;

    #[test]
    fn test() {
        let mut rnt = RustNameTracker::new();
        assert!(rnt.ok_to_use_rust_name("a"));
        assert!(!rnt.ok_to_use_rust_name("a"));
        assert!(rnt.ok_to_use_rust_name("b"));
    }
}
