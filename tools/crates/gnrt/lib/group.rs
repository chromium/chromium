// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use anyhow::Result;

/// Privilege group for a crate. They are ordered with higher values being
/// higher privilege.
#[derive(Copy, Clone, Debug, Hash, PartialEq, Eq, Ord, PartialOrd)]
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

impl Group {
    pub fn new_from_str(s: &str) -> Result<Group> {
        match s {
            "safe" => Ok(Group::Safe),
            "sandbox" => Ok(Group::Sandbox),
            "test" => Ok(Group::Test),
            _ => Err(GroupParseError {}.into()),
        }
    }
}

impl ToString for Group {
    fn to_string(&self) -> String {
        match self {
            Group::Safe => "safe".to_string(),
            Group::Sandbox => "sandbox".to_string(),
            Group::Test => "test".to_string(),
        }
    }
}

#[derive(Debug)]
struct GroupParseError {}
impl std::error::Error for GroupParseError {}
impl std::fmt::Display for GroupParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::result::Result<(), std::fmt::Error> {
        write!(f, "failed ot parse group, should be one of: safe|sandbox|test")
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn parse() {
        assert_eq!(Group::new_from_str("safe").unwrap(), Group::Safe);
        assert_eq!(Group::new_from_str("sandbox").unwrap(), Group::Sandbox);
        assert_eq!(Group::new_from_str("test").unwrap(), Group::Test);
        assert!(Group::new_from_str("oops").is_err());
    }

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
