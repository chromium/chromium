// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use serde::Deserialize;

/// Privilege group for a crate. They are ordered with higher values being
/// higher privilege.
#[derive(Copy, Clone, Debug, Deserialize, Hash, PartialEq, Eq, Ord, PartialOrd)]
#[serde(rename_all = "lowercase")]
pub enum Group {
    /// Test-only code (or for tools that don't contribute directly to the
    /// shipping product).
    Test,
    /// Crates that do not satisfy the Rule of Two, and thus must be used from
    /// a sandboxed process.
    Sandbox,
    /// Crates that satisfy the Rule of Two, and can be used without
    /// restriction from any process.
    Safe,
}

#[derive(Debug)]
pub struct GroupParseError;
impl std::error::Error for GroupParseError {}
impl std::fmt::Display for GroupParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "failed ot parse group, should be one of: safe|sandbox|test")
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn least_privilege() {
        assert_eq!(std::cmp::min(Group::Safe, Group::Safe), Group::Safe);
        assert_eq!(std::cmp::min(Group::Safe, Group::Sandbox), Group::Sandbox);
        assert_eq!(std::cmp::min(Group::Sandbox, Group::Safe), Group::Sandbox);
        assert_eq!(std::cmp::min(Group::Safe, Group::Test), Group::Test);
        assert_eq!(std::cmp::min(Group::Test, Group::Safe), Group::Test);
        assert_eq!(std::cmp::min(Group::Sandbox, Group::Sandbox), Group::Sandbox);
        assert_eq!(std::cmp::min(Group::Sandbox, Group::Test), Group::Test);
        assert_eq!(std::cmp::min(Group::Test, Group::Sandbox), Group::Test);
        assert_eq!(std::cmp::min(Group::Test, Group::Test), Group::Test);
    }
}
